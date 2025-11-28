#ifndef COLOG_LOCK_FREE_QUEUE_H
#define COLOG_LOCK_FREE_QUEUE_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace CoLog {

// Cache line size for avoiding false sharing
#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr std::size_t kCacheLineSize = 64;
#endif

/**
 * @brief A lock-free Multi-Producer Multi-Consumer (MPMC) bounded queue.
 * 
 * This implementation uses a ring buffer with per-slot sequence numbers
 * to achieve lock-free concurrent access from multiple producers and consumers.
 * 
 * Based on Dmitry Vyukov's bounded MPMC queue design.
 */
template <typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(std::size_t capacity)
        : capacity_(next_power_of_two(capacity)),
          mask_(capacity_ - 1),
          buffer_(std::make_unique<Slot[]>(capacity_)) {
        // Initialize sequence numbers
        for (std::size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~LockFreeQueue() = default;

    // Non-copyable, non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;

    /**
     * @brief Try to enqueue an item.
     * @param item The item to enqueue (will be moved).
     * @return true if successful, false if the queue is full.
     */
    bool try_push(T item) {
        Slot* slot;
        std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        while (true) {
            slot = &buffer_[pos & mask_];
            std::size_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);

            if (diff == 0) {
                // Slot is ready for writing
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is full
                return false;
            } else {
                // Another producer got here first, retry
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Write the item and update sequence
        slot->data = std::move(item);
        slot->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to dequeue an item.
     * @return The dequeued item, or std::nullopt if the queue is empty.
     */
    std::optional<T> try_pop() {
        Slot* slot;
        std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        while (true) {
            slot = &buffer_[pos & mask_];
            std::size_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);

            if (diff == 0) {
                // Slot has data ready for reading
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is empty
                return std::nullopt;
            } else {
                // Another consumer got here first, retry
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Read the item and update sequence
        T item = std::move(slot->data);
        slot->sequence.store(pos + capacity_, std::memory_order_release);
        return item;
    }

    /**
     * @brief Check if the queue is empty.
     * @note This is only an approximation in a concurrent environment.
     */
    bool empty() const {
        std::size_t enq = enqueue_pos_.load(std::memory_order_relaxed);
        std::size_t deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq == deq;
    }

    /**
     * @brief Get the approximate size of the queue.
     * @note This is only an approximation in a concurrent environment.
     */
    std::size_t size_approx() const {
        std::size_t enq = enqueue_pos_.load(std::memory_order_relaxed);
        std::size_t deq = dequeue_pos_.load(std::memory_order_relaxed);
        return enq - deq;
    }

    /**
     * @brief Get the capacity of the queue.
     */
    std::size_t capacity() const { return capacity_; }

private:
    struct Slot {
        std::atomic<std::size_t> sequence;
        T data;
    };

    static std::size_t next_power_of_two(std::size_t n) {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    const std::size_t capacity_;
    const std::size_t mask_;
    std::unique_ptr<Slot[]> buffer_;

    // Separate cache lines for producer and consumer positions
    alignas(kCacheLineSize) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> dequeue_pos_{0};

    // Padding to prevent false sharing with adjacent objects
    char padding_[kCacheLineSize - sizeof(std::atomic<std::size_t>)];
};

}  // namespace CoLog

#endif  // COLOG_LOCK_FREE_QUEUE_H


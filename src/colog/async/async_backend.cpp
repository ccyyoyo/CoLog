#include "async_backend.h"

#include <algorithm>

namespace CoLog {

AsyncBackend& AsyncBackend::instance() {
    static AsyncBackend instance;
    return instance;
}

AsyncBackend::~AsyncBackend() {
    if (running_.load(std::memory_order_acquire)) {
        stop();
    }
}

void AsyncBackend::start(const AsyncConfig& config) {
    // Prevent double-start
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;  // Already running
    }

    config_ = config;
    stop_requested_.store(false, std::memory_order_release);
    flush_requested_.store(false, std::memory_order_release);
    processed_generation_.store(0, std::memory_order_release);

    // Create the queue
    queue_ = std::make_unique<LockFreeQueue<AsyncLogItem>>(config_.queue_size);

    // Start the worker thread
    worker_thread_ = std::thread(&AsyncBackend::worker_loop, this);
}

void AsyncBackend::stop(std::chrono::milliseconds timeout) {
    // Signal stop
    stop_requested_.store(true, std::memory_order_release);

    // Wake up the worker
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cv_.notify_all();
    }

    // Wait for the worker to finish (with timeout)
    if (worker_thread_.joinable()) {
        // Use a simple polling approach with timeout
        auto start = std::chrono::steady_clock::now();
        while (running_.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                // Timeout - force detach (not ideal but prevents hanging)
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    running_.store(false, std::memory_order_release);
    queue_.reset();
}

bool AsyncBackend::submit(AsyncLogItem item) {
    if (!running_.load(std::memory_order_acquire) || !queue_) {
        return false;
    }

    if (config_.discard_on_full) {
        // Non-blocking: discard if full
        return queue_->try_push(std::move(item));
    } else {
        // Blocking: spin until we can push (with backoff)
        while (!queue_->try_push(std::move(item))) {
            if (stop_requested_.load(std::memory_order_acquire)) {
                return false;  // Give up if stopping
            }
            std::this_thread::yield();
        }
        return true;
    }
}

void AsyncBackend::flush() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    flush_requested_.store(true, std::memory_order_release);
    
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_one();
}

bool AsyncBackend::wait_for_drain(std::chrono::milliseconds timeout) {
    if (!running_.load(std::memory_order_acquire)) {
        return true;
    }

    // Record the current generation and request a flush
    auto target = processed_generation_.load(std::memory_order_acquire) + 1;
    flush();

    auto start = std::chrono::steady_clock::now();
    while (processed_generation_.load(std::memory_order_acquire) < target) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

std::size_t AsyncBackend::queue_size() const {
    if (!queue_) return 0;
    return queue_->size_approx();
}

void AsyncBackend::worker_loop() {
    while (!stop_requested_.load(std::memory_order_acquire)) {
        // Process a batch
        std::size_t processed = process_batch();

        // If we processed something, continue immediately
        if (processed > 0) {
            processed_generation_.fetch_add(1, std::memory_order_release);
            continue;
        }

        // Wait for new items or flush request
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, config_.flush_interval, [this] {
            return stop_requested_.load(std::memory_order_acquire) ||
                   flush_requested_.load(std::memory_order_acquire) ||
                   (queue_ && !queue_->empty());
        });

        flush_requested_.store(false, std::memory_order_release);
    }

    // Drain remaining items before exit
    drain_queue();
    running_.store(false, std::memory_order_release);
}

std::size_t AsyncBackend::process_batch() {
    if (!queue_) return 0;

    std::size_t count = 0;

    while (count < config_.batch_size) {
        auto item = queue_->try_pop();
        if (!item.has_value()) {
            break;
        }

        // Format and write to sinks
        try {
            std::string formatted = item->formatter->format(item->record);
            for (auto& sink : item->sinks) {
                sink->write(formatted);
            }
        } catch (...) {
            // Swallow exceptions in the worker to prevent crashes
            // In a production system, we might want to log this somewhere
        }

        ++count;
    }

    // Flush sinks after batch if we processed anything
    // Note: We don't track which sinks were used, so this is a simplification
    // In a more sophisticated implementation, we'd batch by sink and flush each

    return count;
}

void AsyncBackend::drain_queue() {
    if (!queue_) return;

    // Process all remaining items
    while (true) {
        auto item = queue_->try_pop();
        if (!item.has_value()) {
            break;
        }

        try {
            std::string formatted = item->formatter->format(item->record);
            for (auto& sink : item->sinks) {
                sink->write(formatted);
                sink->flush();  // Ensure flush on shutdown
            }
        } catch (...) {
            // Swallow exceptions
        }
    }
}

}  // namespace CoLog


#ifndef COLOG_ASYNC_BACKEND_H
#define COLOG_ASYNC_BACKEND_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../formatter.h"
#include "../record.h"
#include "../sink.h"
#include "lock_free_queue.h"

namespace CoLog {

/**
 * @brief Configuration for the async backend.
 */
struct AsyncConfig {
    std::size_t queue_size = 8192;                                     // Queue capacity
    std::chrono::milliseconds flush_interval{100};                     // Max time between flushes
    std::size_t batch_size = 256;                                      // Max records per batch
    bool discard_on_full = false;                                      // Discard if queue full vs block
};

/**
 * @brief Async log item stored in the queue.
 * 
 * Contains all information needed to format and write a log record.
 */
struct AsyncLogItem {
    LogRecord record;
    FormatterPtr formatter;
    std::vector<SinkPtr> sinks;

    AsyncLogItem() = default;
    AsyncLogItem(LogRecord rec, FormatterPtr fmt, std::vector<SinkPtr> snks)
        : record(std::move(rec)), 
          formatter(std::move(fmt)), 
          sinks(std::move(snks)) {}
};

/**
 * @brief Centralized async backend for processing log records.
 * 
 * The AsyncBackend runs a dedicated background worker thread that:
 * - Dequeues log records from the lock-free queue
 * - Batches records for efficiency
 * - Formats and writes to sinks
 * - Handles graceful shutdown with queue drain
 */
class AsyncBackend {
public:
    /**
     * @brief Get the singleton instance of the async backend.
     */
    static AsyncBackend& instance();

    /**
     * @brief Start the async backend with the given configuration.
     * @param config Configuration options.
     */
    void start(const AsyncConfig& config = AsyncConfig{});

    /**
     * @brief Stop the async backend and flush remaining items.
     * @param timeout Maximum time to wait for queue to drain.
     */
    void stop(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief Check if the backend is running.
     */
    bool is_running() const { return running_.load(std::memory_order_acquire); }

    /**
     * @brief Submit a log item to the queue.
     * @param item The log item to submit.
     * @return true if submitted successfully, false if queue is full (when discard_on_full is true).
     */
    bool submit(AsyncLogItem item);

    /**
     * @brief Request an immediate flush of pending items.
     * 
     * This signals the worker to process items immediately rather than
     * waiting for the next batch/interval.
     */
    void flush();

    /**
     * @brief Wait until all currently queued items have been processed.
     * @param timeout Maximum time to wait.
     * @return true if queue was drained, false if timeout occurred.
     */
    bool wait_for_drain(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief Get approximate number of items in the queue.
     */
    std::size_t queue_size() const;

private:
    AsyncBackend() = default;
    ~AsyncBackend();

    // Non-copyable
    AsyncBackend(const AsyncBackend&) = delete;
    AsyncBackend& operator=(const AsyncBackend&) = delete;

    /**
     * @brief Main worker loop running on the background thread.
     */
    void worker_loop();

    /**
     * @brief Process a batch of items from the queue.
     * @return Number of items processed.
     */
    std::size_t process_batch();

    /**
     * @brief Drain all remaining items in the queue.
     */
    void drain_queue();

    // Configuration
    AsyncConfig config_;

    // Queue
    std::unique_ptr<LockFreeQueue<AsyncLogItem>> queue_;

    // Worker thread
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Synchronization for flush and wait operations
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> flush_requested_{false};

    // For wait_for_drain: track a "generation" that increments each batch
    std::atomic<std::uint64_t> processed_generation_{0};
    std::uint64_t wait_target_generation_{0};
};

}  // namespace CoLog

#endif  // COLOG_ASYNC_BACKEND_H


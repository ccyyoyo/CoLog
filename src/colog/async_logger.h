#ifndef COLOG_ASYNC_LOGGER_H
#define COLOG_ASYNC_LOGGER_H

#include <memory>
#include <source_location>
#include <string>
#include <vector>

#include "async/async_backend.h"
#include "formatter.h"
#include "level.h"
#include "pattern_formatter.h"
#include "sink.h"

namespace CoLog {

/**
 * @brief Async logger that uses the centralized AsyncBackend.
 * 
 * This logger provides the same interface as the synchronous Logger,
 * but all formatting and I/O operations happen on a background thread.
 * Log calls return immediately after enqueueing the message.
 */
class AsyncLogger {
public:
    explicit AsyncLogger(std::string name);
    ~AsyncLogger();

    // Non-copyable, movable
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger(AsyncLogger&&) = default;
    AsyncLogger& operator=(AsyncLogger&&) = default;

    /**
     * @brief Core async logging method.
     * 
     * Creates a LogRecord and submits it to the async backend queue.
     * Returns immediately without blocking on I/O.
     */
    void log(LogLevel level, const std::string& message,
             std::source_location loc = std::source_location::current());

    // Convenience methods
    void trace(const std::string& message,
               std::source_location loc = std::source_location::current());
    void debug(const std::string& message,
               std::source_location loc = std::source_location::current());
    void info(const std::string& message,
              std::source_location loc = std::source_location::current());
    void warn(const std::string& message,
              std::source_location loc = std::source_location::current());
    void error(const std::string& message,
               std::source_location loc = std::source_location::current());
    void critical(const std::string& message,
                  std::source_location loc = std::source_location::current());

    // Configuration
    void add_sink(SinkPtr sink);
    void set_formatter(FormatterPtr formatter);
    void set_level(LogLevel level);

    // Accessors
    const std::string& name() const { return name_; }
    LogLevel level() const { return level_; }

    /**
     * @brief Request the async backend to flush pending items.
     * 
     * Note: This does not block. Use flush_wait() for synchronous flush.
     */
    void flush();

    /**
     * @brief Flush and wait for pending items to be processed.
     * @param timeout Maximum time to wait.
     * @return true if all items were flushed, false on timeout.
     */
    bool flush_wait(std::chrono::milliseconds timeout = std::chrono::seconds(5));

private:
    std::string name_;
    LogLevel level_ = LogLevel::Trace;
    std::vector<SinkPtr> sinks_;
    FormatterPtr formatter_;
};

using AsyncLoggerPtr = std::shared_ptr<AsyncLogger>;

/**
 * @brief Initialize the global async backend.
 * 
 * Should be called once at application startup before using async loggers.
 * @param config Configuration for the async backend.
 */
void init_async(const AsyncConfig& config = AsyncConfig{});

/**
 * @brief Shutdown the global async backend.
 * 
 * Flushes all pending log items and stops the background thread.
 * Should be called before application exit.
 * @param timeout Maximum time to wait for pending items.
 */
void shutdown_async(std::chrono::milliseconds timeout = std::chrono::seconds(5));

/**
 * @brief Check if the async backend is running.
 */
bool is_async_running();

}  // namespace CoLog

#endif  // COLOG_ASYNC_LOGGER_H


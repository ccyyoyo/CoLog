#include "async_logger.h"

#include "record.h"

namespace CoLog {

AsyncLogger::AsyncLogger(std::string name)
    : name_(std::move(name)),
      formatter_(std::make_shared<PatternFormatter>()) {}

AsyncLogger::~AsyncLogger() {
    // Optionally flush on destruction
    // Note: We don't wait here to avoid blocking in destructor
    flush();
}

void AsyncLogger::log(LogLevel level, const std::string& message,
                      std::source_location loc) {
    // Early level filtering (fast path - no lock needed)
    if (level < level_) {
        return;
    }

    // Check if backend is running
    if (!AsyncBackend::instance().is_running()) {
        return;  // Silently drop if backend not initialized
    }

    // Create log record (capture timestamp now, not when processed)
    LogRecord record(level, message, name_, loc);

    // Create async item with copies of formatter and sinks
    AsyncLogItem item(std::move(record), formatter_, sinks_);

    // Submit to backend queue
    AsyncBackend::instance().submit(std::move(item));
}

void AsyncLogger::trace(const std::string& message, std::source_location loc) {
    log(LogLevel::Trace, message, loc);
}

void AsyncLogger::debug(const std::string& message, std::source_location loc) {
    log(LogLevel::Debug, message, loc);
}

void AsyncLogger::info(const std::string& message, std::source_location loc) {
    log(LogLevel::Info, message, loc);
}

void AsyncLogger::warn(const std::string& message, std::source_location loc) {
    log(LogLevel::Warn, message, loc);
}

void AsyncLogger::error(const std::string& message, std::source_location loc) {
    log(LogLevel::Error, message, loc);
}

void AsyncLogger::critical(const std::string& message, std::source_location loc) {
    log(LogLevel::Critical, message, loc);
}

void AsyncLogger::add_sink(SinkPtr sink) {
    sinks_.push_back(std::move(sink));
}

void AsyncLogger::set_formatter(FormatterPtr formatter) {
    formatter_ = std::move(formatter);
}

void AsyncLogger::set_level(LogLevel level) {
    level_ = level;
}

void AsyncLogger::flush() {
    AsyncBackend::instance().flush();
}

bool AsyncLogger::flush_wait(std::chrono::milliseconds timeout) {
    return AsyncBackend::instance().wait_for_drain(timeout);
}

// Global async management functions
void init_async(const AsyncConfig& config) {
    AsyncBackend::instance().start(config);
}

void shutdown_async(std::chrono::milliseconds timeout) {
    AsyncBackend::instance().stop(timeout);
}

bool is_async_running() {
    return AsyncBackend::instance().is_running();
}

}  // namespace CoLog


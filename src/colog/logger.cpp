#include "logger.h"

#include "pattern_formatter.h"
#include "record.h"

namespace CoLog {

Logger::Logger(std::string name)
    : name_(std::move(name)),
      formatter_(std::make_shared<PatternFormatter>()) {}

void Logger::log(LogLevel level, const std::string& message,
                 std::source_location loc) {
    // Early level filtering (no lock needed for this check)
    if (level < level_) {
        return;
    }

    // Create log record
    LogRecord record(level, message, name_, loc);

    // Format and write to all sinks
    std::lock_guard<std::mutex> lock(mutex_);
    std::string formatted = formatter_->format(record);

    for (auto& sink : sinks_) {
        sink->write(formatted);
    }
}

void Logger::trace(const std::string& message, std::source_location loc) {
    log(LogLevel::Trace, message, loc);
}

void Logger::debug(const std::string& message, std::source_location loc) {
    log(LogLevel::Debug, message, loc);
}

void Logger::info(const std::string& message, std::source_location loc) {
    log(LogLevel::Info, message, loc);
}

void Logger::warn(const std::string& message, std::source_location loc) {
    log(LogLevel::Warn, message, loc);
}

void Logger::error(const std::string& message, std::source_location loc) {
    log(LogLevel::Error, message, loc);
}

void Logger::critical(const std::string& message, std::source_location loc) {
    log(LogLevel::Critical, message, loc);
}

void Logger::add_sink(SinkPtr sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::set_formatter(FormatterPtr formatter) {
    std::lock_guard<std::mutex> lock(mutex_);
    formatter_ = std::move(formatter);
}

void Logger::set_level(LogLevel level) {
    level_ = level;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

}  // namespace CoLog


#ifndef COLOG_LOGGER_H
#define COLOG_LOGGER_H

#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <vector>

#include "formatter.h"
#include "level.h"
#include "sink.h"

namespace CoLog {

class Logger {
public:
    explicit Logger(std::string name);
    ~Logger() = default;

    // Non-copyable, movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;

    // Core logging method
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

    // Flush all sinks
    void flush();

private:
    std::string name_;
    LogLevel level_ = LogLevel::Trace;
    std::vector<SinkPtr> sinks_;
    FormatterPtr formatter_;
    std::mutex mutex_;
};

using LoggerPtr = std::shared_ptr<Logger>;

}  // namespace CoLog

#endif  // COLOG_LOGGER_H


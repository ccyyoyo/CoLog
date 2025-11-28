#ifndef COLOG_RECORD_H
#define COLOG_RECORD_H

#include <chrono>
#include <source_location>
#include <string>
#include <string_view>

#include "level.h"

namespace CoLog {

struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
    std::string logger_name;  // Changed from string_view to avoid dangling reference
    std::source_location location;

    // Default constructor for container compatibility
    LogRecord() 
        : timestamp(std::chrono::system_clock::now()),
          level(LogLevel::Info),
          message(),
          logger_name(),
          location(std::source_location::current()) {}

    LogRecord(LogLevel lvl, std::string msg, std::string_view name,
              std::source_location loc = std::source_location::current())
        : timestamp(std::chrono::system_clock::now()),
          level(lvl),
          message(std::move(msg)),
          logger_name(name),
          location(loc) {}
};

}  // namespace CoLog

#endif  // COLOG_RECORD_H

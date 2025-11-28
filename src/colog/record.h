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
    std::string_view logger_name;
    std::source_location location;

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

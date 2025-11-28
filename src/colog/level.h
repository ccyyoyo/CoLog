#ifndef COLOG_LEVEL_H
#define COLOG_LEVEL_H

#include <string_view>

namespace CoLog {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6  // Used to disable logging
};

constexpr std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::Off:      return "OFF";
    }
    return "UNKNOWN";
}

}  // namespace CoLog

#endif  // COLOG_LEVEL_H

#include "pattern_formatter.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace CoLog {

std::string PatternFormatter::format(const LogRecord& record) {
    std::ostringstream oss;

    // Format timestamp: [2024-01-01 12:00:00.123]
    auto time_t_val = std::chrono::system_clock::to_time_t(record.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  record.timestamp.time_since_epoch()) %
              1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    oss << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count() << "] ";

    // Format level: [INFO]
    oss << '[' << to_string(record.level) << "] ";

    // Format logger name: [main]
    oss << '[' << record.logger_name << "] ";

    // Message
    oss << record.message << '\n';

    return oss.str();
}

}  // namespace CoLog

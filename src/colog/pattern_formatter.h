#ifndef COLOG_PATTERN_FORMATTER_H
#define COLOG_PATTERN_FORMATTER_H

#include "formatter.h"

namespace CoLog {

// PatternFormatter formats log records as:
// [2024-01-01 12:00:00.123] [INFO] [logger_name] message
class PatternFormatter : public IFormatter {
public:
    PatternFormatter() = default;
    ~PatternFormatter() override = default;

    std::string format(const LogRecord& record) override;
};

}  // namespace CoLog

#endif  // COLOG_PATTERN_FORMATTER_H

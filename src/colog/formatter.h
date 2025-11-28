#ifndef COLOG_FORMATTER_H
#define COLOG_FORMATTER_H

#include <memory>
#include <string>

#include "record.h"

namespace CoLog {

class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const LogRecord& record) = 0;
};

using FormatterPtr = std::shared_ptr<IFormatter>;

}  // namespace CoLog

#endif  // COLOG_FORMATTER_H

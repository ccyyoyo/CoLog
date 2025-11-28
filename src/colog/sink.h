#ifndef COLOG_SINK_H
#define COLOG_SINK_H

#include <memory>
#include <string_view>

namespace CoLog {

class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(std::string_view message) = 0;
    virtual void flush() = 0;
};

using SinkPtr = std::shared_ptr<ISink>;

}  // namespace CoLog

#endif  // COLOG_SINK_H


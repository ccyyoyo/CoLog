#ifndef COLOG_CONSOLE_SINK_H
#define COLOG_CONSOLE_SINK_H

#include <mutex>

#include "sink.h"

namespace CoLog {

class ConsoleSink : public ISink {
public:
    ConsoleSink() = default;
    ~ConsoleSink() override = default;

    void write(std::string_view message) override;
    void flush() override;

private:
    std::mutex mutex_;
};

}  // namespace CoLog

#endif  // COLOG_CONSOLE_SINK_H


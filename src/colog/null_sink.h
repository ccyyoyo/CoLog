#ifndef COLOG_NULL_SINK_H
#define COLOG_NULL_SINK_H

#include "sink.h"

namespace CoLog {

/**
 * @brief A sink that discards all output.
 * 
 * Useful for benchmarking the pure overhead of the logging system
 * without any actual I/O operations.
 */
class NullSink : public ISink {
public:
    NullSink() = default;
    ~NullSink() override = default;

    void write(std::string_view /*message*/) override {
        // Intentionally empty - discard all output
    }

    void flush() override {
        // Nothing to flush
    }
};

}  // namespace CoLog

#endif  // COLOG_NULL_SINK_H


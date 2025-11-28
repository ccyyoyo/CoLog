#ifndef COLOG_FILE_SINK_H
#define COLOG_FILE_SINK_H

#include <fstream>
#include <mutex>
#include <string>

#include "sink.h"

namespace CoLog {

class FileSink : public ISink {
public:
    explicit FileSink(const std::string& filename, bool append = true);
    ~FileSink() override;

    void write(std::string_view message) override;
    void flush() override;

    bool is_open() const;

private:
    std::ofstream file_;
    mutable std::mutex mutex_;
};

}  // namespace CoLog

#endif  // COLOG_FILE_SINK_H


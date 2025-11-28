#include "file_sink.h"

#include <stdexcept>

namespace CoLog {

FileSink::FileSink(const std::string& filename, bool append) {
    std::ios::openmode mode = append 
        ? (std::ios::out | std::ios::app) 
        : std::ios::out;

    file_.open(filename, mode);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

FileSink::~FileSink() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void FileSink::write(std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_ << message;
    }
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

bool FileSink::is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return file_.is_open();
}

}  // namespace CoLog


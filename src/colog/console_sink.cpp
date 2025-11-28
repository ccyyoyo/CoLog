#include "console_sink.h"

#include <iostream>

namespace CoLog {

void ConsoleSink::write(std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << message;
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
}

}  // namespace CoLog


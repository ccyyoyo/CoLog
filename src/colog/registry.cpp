#include "registry.h"

#include "console_sink.h"

namespace CoLog {

Registry& Registry::instance() {
    static Registry instance;
    return instance;
}

LoggerPtr Registry::get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    // Create new logger
    auto logger = std::make_shared<Logger>(name);
    loggers_[name] = logger;
    return logger;
}

void Registry::set_default(LoggerPtr logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_logger_ = std::move(logger);
}

LoggerPtr Registry::get_default() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!default_logger_) {
        // Create a default logger with console sink
        default_logger_ = std::make_shared<Logger>("default");
        default_logger_->add_sink(std::make_shared<ConsoleSink>());
        loggers_["default"] = default_logger_;
    }

    return default_logger_;
}

void Registry::flush_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, logger] : loggers_) {
        logger->flush();
    }
}

void Registry::drop(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.erase(name);
}

void Registry::drop_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.clear();
    default_logger_.reset();
}

// Convenience free functions
LoggerPtr get_logger(const std::string& name) {
    return Registry::instance().get(name);
}

LoggerPtr get_default_logger() {
    return Registry::instance().get_default();
}

void set_default_logger(LoggerPtr logger) {
    Registry::instance().set_default(std::move(logger));
}

}  // namespace CoLog


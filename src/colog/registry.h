#ifndef COLOG_REGISTRY_H
#define COLOG_REGISTRY_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "logger.h"

namespace CoLog {

class Registry {
public:
    // Get the singleton instance
    static Registry& instance();

    // Get or create a logger by name
    LoggerPtr get(const std::string& name);

    // Set the default logger
    void set_default(LoggerPtr logger);

    // Get the default logger (creates one if not set)
    LoggerPtr get_default();

    // Flush all loggers
    void flush_all();

    // Drop a logger by name
    void drop(const std::string& name);

    // Drop all loggers
    void drop_all();

private:
    Registry() = default;
    ~Registry() = default;

    // Non-copyable
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    std::unordered_map<std::string, LoggerPtr> loggers_;
    LoggerPtr default_logger_;
    std::mutex mutex_;
};

// Convenience free functions
LoggerPtr get_logger(const std::string& name);
LoggerPtr get_default_logger();
void set_default_logger(LoggerPtr logger);

}  // namespace CoLog

#endif  // COLOG_REGISTRY_H


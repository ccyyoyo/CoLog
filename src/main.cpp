#include "colog/colog.h"

int main() {
    // Create a logger with both console and file sinks
    auto logger = CoLog::get_logger("main");
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    logger->add_sink(std::make_shared<CoLog::FileSink>("app.log"));

    // Set as default logger
    CoLog::set_default_logger(logger);

    // Demonstrate all log levels
    logger->trace("This is a trace message");
    logger->debug("This is a debug message");
    logger->info("Hello World from CoLog!");
    logger->warn("This is a warning");
    logger->error("This is an error");
    logger->critical("This is critical!");

    // Using default logger
    auto default_log = CoLog::get_default_logger();
    default_log->info("Using the default logger");

    // Flush to ensure all logs are written
    logger->flush();

    return 0;
}

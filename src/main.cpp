#include "colog/colog.h"

#include <chrono>
#include <iostream>
#include <thread>

void demo_sync_logger() {
    std::cout << "\n=== Synchronous Logger Demo ===\n\n";

    // Create a logger with both console and file sinks
    auto logger = CoLog::get_logger("sync");
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    logger->add_sink(std::make_shared<CoLog::FileSink>("sync.log"));

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
}

void demo_async_logger() {
    std::cout << "\n=== Asynchronous Logger Demo ===\n\n";

    // Initialize the async backend
    CoLog::AsyncConfig config;
    config.queue_size = 8192;
    config.batch_size = 256;
    config.flush_interval = std::chrono::milliseconds(100);
    CoLog::init_async(config);

    // Create an async logger
    auto logger = std::make_shared<CoLog::AsyncLogger>("async");
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    logger->add_sink(std::make_shared<CoLog::FileSink>("async.log"));

    // Log messages asynchronously
    logger->trace("Async trace message");
    logger->debug("Async debug message");
    logger->info("Hello World from Async CoLog!");
    logger->warn("Async warning");
    logger->error("Async error");
    logger->critical("Async critical!");

    // Demonstrate non-blocking nature
    logger->info("This returns immediately - I/O happens in background!");

    // Wait for all messages to be processed
    logger->flush_wait();
}

void demo_async_throughput() {
    std::cout << "\n=== Async Throughput Test ===\n\n";

    // Use NullSink for pure throughput measurement
    auto logger = std::make_shared<CoLog::AsyncLogger>("benchmark");
    logger->add_sink(std::make_shared<CoLog::NullSink>());

    constexpr int NUM_MESSAGES = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        logger->info("Benchmark message number " + std::to_string(i));
    }

    auto enqueue_time = std::chrono::high_resolution_clock::now();

    // Wait for all to be processed
    logger->flush_wait(std::chrono::seconds(30));

    auto end = std::chrono::high_resolution_clock::now();

    auto enqueue_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        enqueue_time - start);
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start);

    double enqueue_rate = static_cast<double>(NUM_MESSAGES) / 
                          (enqueue_duration.count() / 1000.0);
    double total_rate = static_cast<double>(NUM_MESSAGES) / 
                        (total_duration.count() / 1000.0);

    std::cout << "Enqueued " << NUM_MESSAGES << " messages in " 
              << enqueue_duration.count() << " ms\n";
    std::cout << "Enqueue rate: " << static_cast<int>(enqueue_rate) 
              << " msg/sec\n";
    std::cout << "Total time (including processing): " 
              << total_duration.count() << " ms\n";
    std::cout << "Effective throughput: " << static_cast<int>(total_rate) 
              << " msg/sec\n";
}

int main() {
    std::cout << "CoLog - High-Performance C++20 Logging Library\n";
    std::cout << "=============================================\n";

    // Demo 1: Synchronous logging (Phase 1)
    demo_sync_logger();

    // Demo 2: Asynchronous logging (Phase 2)
    demo_async_logger();

    // Demo 3: Throughput test
    demo_async_throughput();

    // Graceful shutdown
    CoLog::shutdown_async();

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

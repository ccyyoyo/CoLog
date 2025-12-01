# CoLog: High-Performance C++20 Coroutine Logger

A modern, high-performance C++ logging library designed to demonstrate the power of **C++20 Coroutines**, **Asynchronous I/O**, and **Lock-free Data Structures**.

This project aims to be a production-grade logger that rivals established libraries like `spdlog` or `glog` in terms of architecture and performance, while serving as a comprehensive showcase of modern C++ systems programming.

## 📊 Current Status

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Baseline Synchronous Logger | ✅ Complete |
| Phase 2 | Async Core (C++20 Coroutines) | 🔲 Planned |
| Phase 3 | Benchmarking & Optimization | 🔲 Planned |
| Phase 4 | Advanced Features | 🔲 Planned |
| Phase 5 | Polish & Release | 🔲 Planned |

## 🎯 Project Goals

- **Performance**: High throughput and low latency using lock-free queues and asynchronous flushing.
- **Modern C++**: Heavy utilization of C++20 features (Coroutines, Concepts, Modules integration).
- **Architecture**: Follows mainstream logging design philosophy (Logger → Formatter → Sink).
- **Benchmarking**: Rigorous performance comparison against industry standards.
- **Cross-Platform**: Full support for Windows, Linux, and macOS via CMake.

## ✨ Key Features

### 1. Synchronous & Asynchronous Modes
- **Baseline Synchronous Logger**: Thread-safe blocking I/O for safety and debugging.
- **High-Performance Async Logger** (coming soon):
  - Non-blocking `log()` calls.
  - Background worker threads utilizing C++20 Coroutines.
  - SPSC (Single Producer Single Consumer) or MPMC lock-free queues for low-latency messaging.

### 2. Flexible Architecture
- **Sink Support**: File, Console, and Null sinks (Network sink planned).
- **Formatter Support**: Pattern-based text formatting (JSON formatter planned).
- **Level Filtering**: Zero-cost abstraction for filtering logs at the call site.

### 3. Comprehensive Benchmarking (Planned)
- Built-in `logger-bench` tool to measure throughput, latency (p95/p99), and queue depth.
- Comparison suites against `spdlog` and `glog`.

## 🛠️ Tech Stack

- **Language**: C++20
- **Build System**: CMake 3.20+
- **Concurrency**: `std::mutex` (sync), `std::coroutine`, `std::jthread`, `std::atomic` (async)
- **I/O**: `std::fstream` (baseline), platform-specific async I/O (planned)
- **CI/CD**: GitHub Actions (Windows/Linux/macOS)

## 📂 Documentation

- [**Phase1 Implementation**](docs/PHASE1.md): 詳細說明 Phase1 同步日誌系統的實作、技術細節和架構設計（繁體中文）
- [**Phase2 Implementation**](docs/PHASE2.md): 詳細說明 Phase2 非同步日誌系統的實作、無鎖佇列、批次處理等進階技術（繁體中文）
- [**Architecture Design**](docs/ARCHITECTURE.md): Detailed breakdown of the Logger-Formatter-Sink model and Async Backend.
- [**Benchmark Plan**](docs/BENCHMARK_PLAN.md): Experimental design, variables, metrics, and scenarios.
- [**Roadmap**](TODO.md): Development phases from baseline to advanced features.

## 🚀 Getting Started

### Prerequisites
- C++20 compliant compiler (GCC 10+, Clang 11+, MSVC 19.29+)
- CMake 3.20+

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Run Demo

```bash
./bin/Release/colog_demo   # Windows
./bin/colog_demo           # Linux/macOS
```

### Usage Example

```cpp
#include "colog/colog.h"

int main() {
    // Create a logger with console and file sinks
    auto logger = CoLog::get_logger("main");
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    logger->add_sink(std::make_shared<CoLog::FileSink>("app.log"));

    // Log messages at different levels
    logger->trace("Detailed trace info");
    logger->debug("Debug information");
    logger->info("Hello, CoLog!");
    logger->warn("Warning: something might be wrong");
    logger->error("Error occurred");
    logger->critical("Critical failure!");

    // Set minimum log level (filter out Trace and Debug)
    logger->set_level(CoLog::LogLevel::Info);

    // Use the global default logger
    CoLog::set_default_logger(logger);
    CoLog::get_default_logger()->info("Using default logger");

    // Flush to ensure all logs are written
    logger->flush();

    return 0;
}
```

### Output Format

```
[2025-11-28 12:00:00.123] [INFO] [main] Hello, CoLog!
[2025-11-28 12:00:00.124] [WARN] [main] Warning: something might be wrong
```

## 📁 Project Structure

```
CoLog/
├── src/
│   ├── colog/
│   │   ├── colog.h              # Main include header
│   │   ├── level.h              # LogLevel enum
│   │   ├── record.h             # LogRecord struct
│   │   ├── formatter.h          # IFormatter interface
│   │   ├── pattern_formatter.h/.cpp
│   │   ├── sink.h               # ISink interface
│   │   ├── file_sink.h/.cpp
│   │   ├── console_sink.h/.cpp
│   │   ├── logger.h/.cpp
│   │   └── registry.h/.cpp
│   └── main.cpp                 # Demo application
├── docs/
│   ├── ARCHITECTURE.md
│   └── BENCHMARK_PLAN.md
├── CMakeLists.txt
├── TODO.md
└── README.md
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

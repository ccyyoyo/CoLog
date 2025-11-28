# Benchmark Design & Strategy

The benchmark suite is a critical component of CoLog, used to verify performance claims and compare against industry standards like `spdlog` and `glog`.

## 🧪 Experimental Design

### 1. Test Scenarios
We evaluate the logger under various conditions to understand its behavior fully.

| Scenario | Description | Purpose |
|----------|-------------|---------|
| **Baseline** | 1 Thread, Sync Logger | Establish absolute minimum overhead. |
| **High Throughput** | 1 Thread, Async Logger, Null Sink | Measure pure enqueue + formatting speed. |
| **Contention** | 8 Threads, 1 Logger Instance | Simulate high-concurrency server environment. |
| **Real-world** | 4 Threads, File Sink, 1KB msg | Simulate typical application usage. |
| **Burst** | 100k logs in strict loop | Test queue capacity and backpressure. |

### 2. Variables (Knobs)
- **Thread Count**: 1, 2, 4, 8, 16, 32.
- **Queue Size**: 4k, 64k, 1M slots.
- **Queue Strategy**: 
    - `Block`: Wait when full (safe).
    - `Drop`: Discard new logs (real-time).
    - `Overrun`: Overwrite oldest (ring buffer).
- **Backend**: Synchronous vs. Asynchronous (Coroutine).

### 3. Key Metrics
- **Throughput**: Log messages processed per second.
- **Latency**: Time taken for the `log()` call to return (p50, p95, p99).
- **Flush Time**: Time taken for the background worker to clear the queue.
- **Memory Usage**: Peak heap allocation during stress tests.

## 🏗️ Benchmark System Architecture

The benchmarking infrastructure is split into a Runner and an Analyzer.

### Benchmark Runner (`logger-bench`)
A C++ application responsible for generating load.
- **Role**: Spawns worker threads, executes logging patterns, collects raw timestamps.
- **Output**: JSON/CSV file with raw data points.

### Result Analyzer (`tools-analyzer`)
A Python/C++ tool to process the raw data.
- **Role**: Calculates percentiles, generates charts, compares runs.
- **Output**: Markdown reports, PNG plots.

## 📊 Comparison Targets

| Library | Type | Comparison Goal |
|---------|------|-----------------|
| **spdlog** | Sync/Async | The gold standard for C++ logging speed. |
| **glog** | Sync | Google's robust logging library. |
| **Boost.Log**| Sync/Async | Enterprise standard (often heavier). |

## 📝 Artifacts

Each benchmark run produces a report containing:
1. System Info (OS, CPU, Compiler).
2. Configuration (Buffer size, thread count).
3. Result Table (Ops/sec, Latency).
4. Graphs (Scalability curves).


# CoLog Development Roadmap

## Phase 1: Baseline (Synchronous Logger)
- [x] Define public `Logger` interface (`log`, `info`, `warn`).
- [x] Implement `Formatter` interface and a basic `PatternFormatter` (Text).
- [x] Implement `Sink` interface and `FileSink` (std::fstream).
- [x] Implement `ConsoleSink` (std::cout).
- [x] Implement global Logger Registry (Singleton/Static access).
- [x] Add thread safety (std::mutex) for the synchronous path.
- [x] **Milestone**: Write a "Hello World" log to a file.

## Phase 2: Async Core (C++20 Coroutines)
- [ ] Design the `AsyncBackend` class.
- [ ] Implement `LockFreeQueue` (SPSC or MPMC wrapper).
- [ ] Create the Background Worker using `std::jthread` or Coroutines.
- [ ] Implement the `flush` mechanism (batch processing).
- [ ] Integrate `AsyncBackend` with the `Logger` frontend.
- [ ] Handle graceful shutdown (flush queue on exit).
- [ ] **Milestone**: Non-blocking logging with 1M+ msg/sec throughput.

## Phase 3: Benchmarking & Optimization
- [ ] Create `logger-bench` executable (Google Benchmark or custom).
- [ ] Implement `NullSink` for pure overhead measurement.
- [ ] Run "Sync vs Async" comparison.
- [ ] Run "CoLog vs spdlog" comparison.
- [ ] Optimize queue strategy (padding to avoid false sharing).

## Phase 4: Advanced Features
- [ ] **Structured Logging**: Add JSON formatter.
- [ ] **Cross-Platform**: Verify build on Linux/macOS.
- [ ] **CI/CD**: Setup GitHub Actions pipeline.
- [ ] **Rotation**: Implement file rotation (by size or date).

## Phase 5: Polish & Release
- [ ] Write detailed usage documentation.
- [ ] Generate Doxygen API docs.
- [ ] Release v0.1.0.

# Phase2: 非同步日誌系統實作文件

## 📋 概述

Phase2 實作了 CoLog 的**非同步日誌系統**（Asynchronous Logger），這是 Phase1 同步日誌系統的高效能版本。Phase2 的核心目標是實現**非阻塞的日誌呼叫**，讓 `log()` 方法立即返回，而實際的格式化和 I/O 操作在背景執行緒中進行。

### 完成的功能

- ✅ **AsyncLogger**：非同步日誌器，提供與同步 Logger 相同的介面
- ✅ **AsyncBackend**：集中式非同步後端，管理背景工作執行緒
- ✅ **LockFreeQueue**：無鎖多生產者多消費者（MPMC）佇列
- ✅ **批量處理**：批次處理多條日誌，提高吞吐量
- ✅ **優雅關閉**：確保所有待處理日誌在關閉前完成
- ✅ **NullSink**：用於效能測試的空輸出目標
- ✅ **可配置參數**：佇列大小、批次大小、刷新間隔等

---

## 🏗️ 架構設計

Phase2 在 Phase1 的三層架構基礎上，新增了**非同步後端層**：

```
使用者程式碼
   ↓
AsyncLogger（非同步日誌器）← 前端介面，log() 立即返回
   ↓
LockFreeQueue（無鎖佇列）← 生產者：多個執行緒可以同時寫入
   ↓
AsyncBackend（非同步後端）← 背景工作執行緒
   ↓
Formatter + Sink ← 在背景執行緒中執行格式化和寫入
```

### 為什麼需要非同步日誌？

**Phase1 的問題**：
- 每次 `log()` 呼叫都會阻塞，直到 I/O 完成
- 多執行緒環境下，鎖競爭成為瓶頸
- 檔案 I/O 會阻塞呼叫執行緒，影響應用程式效能

**Phase2 的解決方案**：
- `log()` 呼叫立即返回，不等待 I/O
- 使用無鎖佇列，減少鎖競爭
- 背景執行緒專門處理 I/O，不影響主執行緒

---

## 🔧 核心元件詳解

### 1. AsyncLogger（非同步日誌器）

**位置**：`src/colog/async_logger.h` 和 `src/colog/async_logger.cpp`

`AsyncLogger` 提供與同步 `Logger` 相同的介面，但所有操作都是非阻塞的。

#### 關鍵技術點

**a) 非阻塞的 log() 方法**

```cpp
void AsyncLogger::log(LogLevel level, const std::string& message,
                      std::source_location loc) {
    // 早期級別過濾（快速路徑，無需鎖）
    if (level < level_) {
        return;
    }

    // 檢查後端是否運行
    if (!AsyncBackend::instance().is_running()) {
        return;  // 如果後端未初始化，靜默丟棄
    }

    // 建立日誌記錄（立即捕獲時間戳）
    LogRecord record(level, message, name_, loc);

    // 建立非同步項目，包含 formatter 和 sinks 的副本
    AsyncLogItem item(std::move(record), formatter_, sinks_);

    // 提交到後端佇列（立即返回）
    AsyncBackend::instance().submit(std::move(item));
}
```

**設計要點**：
- **時間戳在建立時捕獲**：確保時間戳反映實際呼叫時間，而非處理時間
- **複製 Formatter 和 Sinks**：每個 `AsyncLogItem` 包含自己的 formatter 和 sinks 副本，避免共享狀態的執行緒安全問題
- **立即返回**：`submit()` 只是將項目加入佇列，不等待處理完成

**b) flush_wait() 方法**

```cpp
bool AsyncLogger::flush_wait(std::chrono::milliseconds timeout = 
                              std::chrono::seconds(5)) {
    return AsyncBackend::instance().wait_for_drain(timeout);
}
```

**為什麼需要這個方法？**
- 在某些情況下（如應用程式關閉），需要確保所有日誌都已寫入
- `flush()` 只是請求刷新，不等待完成
- `flush_wait()` 會阻塞直到所有待處理項目完成

### 2. AsyncBackend（非同步後端）

**位置**：`src/colog/async/async_backend.h` 和 `src/colog/async/async_backend.cpp`

`AsyncBackend` 是整個非同步系統的核心，採用**單例模式**，負責：
- 管理無鎖佇列
- 運行背景工作執行緒
- 批次處理日誌項目
- 處理優雅關閉

#### 關鍵技術點

**a) 單例模式與執行緒安全初始化**

```cpp
AsyncBackend& AsyncBackend::instance() {
    static AsyncBackend instance;  // C++11 保證執行緒安全
    return instance;
}
```

**b) 啟動和停止**

```cpp
void AsyncBackend::start(const AsyncConfig& config) {
    // 防止重複啟動
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, 
                                          std::memory_order_acq_rel)) {
        return;  // 已經在運行
    }

    config_ = config;
    stop_requested_.store(false, std::memory_order_release);
    
    // 建立佇列
    queue_ = std::make_unique<LockFreeQueue<AsyncLogItem>>(config_.queue_size);
    
    // 啟動工作執行緒
    worker_thread_ = std::thread(&AsyncBackend::worker_loop, this);
}
```

**使用 `compare_exchange_strong`**：
- **原子操作**：確保只有一個執行緒能成功啟動後端
- **記憶體順序**：`memory_order_acq_rel` 確保啟動操作的順序性

**c) 工作執行緒迴圈**

```cpp
void AsyncBackend::worker_loop() {
    while (!stop_requested_.load(std::memory_order_acquire)) {
        // 處理一批項目
        std::size_t processed = process_batch();

        // 如果處理了項目，立即繼續（保持高吞吐量）
        if (processed > 0) {
            processed_generation_.fetch_add(1, std::memory_order_release);
            continue;
        }

        // 等待新項目或刷新請求
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, config_.flush_interval, [this] {
            return stop_requested_.load(std::memory_order_acquire) ||
                   flush_requested_.load(std::memory_order_acquire) ||
                   (queue_ && !queue_->empty());
        });

        flush_requested_.store(false, std::memory_order_release);
    }

    // 關閉前清空佇列
    drain_queue();
    running_.store(false, std::memory_order_release);
}
```

**設計策略**：
- **忙等待優先**：如果有項目，立即處理，不等待
- **條件變數等待**：佇列為空時，使用條件變數等待，避免 CPU 空轉
- **超時刷新**：即使沒有新項目，也會定期刷新（根據 `flush_interval`）

**d) 批次處理**

```cpp
std::size_t AsyncBackend::process_batch() {
    if (!queue_) return 0;

    std::size_t count = 0;

    // 一次處理最多 batch_size 個項目
    while (count < config_.batch_size) {
        auto item = queue_->try_pop();
        if (!item.has_value()) {
            break;  // 佇列為空
        }

        // 格式化和寫入
        try {
            std::string formatted = item->formatter->format(item->record);
            for (auto& sink : item->sinks) {
                sink->write(formatted);
            }
        } catch (...) {
            // 吞掉異常，防止工作執行緒崩潰
        }

        ++count;
    }

    return count;
}
```

**批次處理的優勢**：
- **減少系統呼叫**：一次處理多條日誌，減少 I/O 系統呼叫次數
- **提高快取效率**：連續處理多個項目，提高 CPU 快取命中率
- **平衡延遲和吞吐量**：批次大小可配置，在延遲和吞吐量之間取得平衡

**e) 優雅關閉**

```cpp
void AsyncBackend::stop(std::chrono::milliseconds timeout) {
    // 發送停止信號
    stop_requested_.store(true, std::memory_order_release);

    // 喚醒工作執行緒
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cv_.notify_all();
    }

    // 等待工作執行緒完成（帶超時）
    if (worker_thread_.joinable()) {
        auto start = std::chrono::steady_clock::now();
        while (running_.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                break;  // 超時
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    running_.store(false, std::memory_order_release);
    queue_.reset();
}
```

**關閉流程**：
1. 設置 `stop_requested_` 標誌
2. 喚醒工作執行緒
3. 工作執行緒處理完所有項目後退出
4. 主執行緒等待工作執行緒結束（帶超時保護）

### 3. LockFreeQueue（無鎖佇列）

**位置**：`src/colog/async/lock_free_queue.h`

`LockFreeQueue` 是一個**多生產者多消費者（MPMC）**無鎖佇列，基於 Dmitry Vyukov 的設計。

#### 關鍵技術點

**a) 環形緩衝區設計**

```cpp
template <typename T>
class LockFreeQueue {
private:
    struct Slot {
        std::atomic<std::size_t> sequence;  // 序號
        T data;                              // 實際資料
    };

    const std::size_t capacity_;
    const std::size_t mask_;  // 用於快速取模（capacity 必須是 2 的冪）
    std::unique_ptr<Slot[]> buffer_;
    
    alignas(kCacheLineSize) std::atomic<std::size_t> enqueue_pos_{0};
    alignas(kCacheLineSize) std::atomic<std::size_t> dequeue_pos_{0};
};
```

**設計要點**：
- **容量必須是 2 的冪**：使用位元運算 `pos & mask_` 代替 `pos % capacity_`，提高效率
- **每個槽位有序號**：用序號判斷槽位是否可用
- **快取行對齊**：生產者和消費者的位置指標分別對齊到不同的快取行，避免**偽共享（False Sharing）**

**b) 偽共享（False Sharing）的避免**

```cpp
// 快取行大小
#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr std::size_t kCacheLineSize = 64;  // 大多數現代 CPU 的快取行大小
#endif

// 分別對齊到不同的快取行
alignas(kCacheLineSize) std::atomic<std::size_t> enqueue_pos_{0};
alignas(kCacheLineSize) std::atomic<std::size_t> dequeue_pos_{0};
```

**什麼是偽共享？**
- 當兩個執行緒頻繁寫入同一快取行的不同變數時，會導致快取行在 CPU 核心間頻繁傳輸
- 即使變數在邏輯上無關，也會造成效能下降
- **解決方法**：將頻繁寫入的變數對齊到不同的快取行

**c) 無鎖的 push 操作**

```cpp
bool try_push(T item) {
    Slot* slot;
    std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

    while (true) {
        slot = &buffer_[pos & mask_];
        std::size_t seq = slot->sequence.load(std::memory_order_acquire);
        auto diff = static_cast<std::ptrdiff_t>(seq) - 
                    static_cast<std::ptrdiff_t>(pos);

        if (diff == 0) {
            // 槽位可用，嘗試取得位置
            if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed)) {
                break;  // 成功取得位置
            }
        } else if (diff < 0) {
            // 佇列已滿（消費者還沒讀取）
            return false;
        } else {
            // 其他生產者先到了，重試
            pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
    }

    // 寫入資料並更新序號
    slot->data = std::move(item);
    slot->sequence.store(pos + 1, std::memory_order_release);
    return true;
}
```

**無鎖演算法解析**：
1. **讀取當前位置**：`enqueue_pos_` 是原子變數
2. **檢查槽位序號**：
   - `seq == pos`：槽位可用
   - `seq < pos`：佇列已滿
   - `seq > pos`：其他生產者先到，重試
3. **CAS 操作**：使用 `compare_exchange_weak` 原子地更新位置
4. **寫入資料**：使用 `memory_order_release` 確保寫入順序

**記憶體順序說明**：
- `memory_order_relaxed`：只保證原子性，不保證順序
- `memory_order_acquire`：讀取操作，確保後續操作不會重排到它之前
- `memory_order_release`：寫入操作，確保前面的操作不會重排到它之後
- `acquire-release` 配對：形成同步點，確保資料可見性

**d) 無鎖的 pop 操作**

```cpp
std::optional<T> try_pop() {
    Slot* slot;
    std::size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

    while (true) {
        slot = &buffer_[pos & mask_];
        std::size_t seq = slot->sequence.load(std::memory_order_acquire);
        auto diff = static_cast<std::ptrdiff_t>(seq) - 
                    static_cast<std::ptrdiff_t>(pos + 1);

        if (diff == 0) {
            // 槽位有資料，嘗試取得位置
            if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed)) {
                break;
            }
        } else if (diff < 0) {
            // 佇列為空
            return std::nullopt;
        } else {
            // 其他消費者先到了，重試
            pos = dequeue_pos_.load(std::memory_order_relaxed);
        }
    }

    // 讀取資料並更新序號
    T item = std::move(slot->data);
    slot->sequence.store(pos + capacity_, std::memory_order_release);
    return item;
}
```

**注意**：序號更新為 `pos + capacity_`，這是為了區分「已讀取」和「未寫入」的狀態。

### 4. AsyncConfig（配置結構）

```cpp
struct AsyncConfig {
    std::size_t queue_size = 8192;                    // 佇列容量
    std::chrono::milliseconds flush_interval{100};    // 最大刷新間隔
    std::size_t batch_size = 256;                    // 每批最大記錄數
    bool discard_on_full = false;                     // 佇列滿時是否丟棄（vs 阻塞）
};
```

**配置參數說明**：
- **queue_size**：佇列容量，影響記憶體使用和背壓處理
- **flush_interval**：即使佇列為空，也會定期刷新（保證低延遲）
- **batch_size**：批次大小，影響吞吐量和延遲的平衡
- **discard_on_full**：
  - `false`：佇列滿時阻塞（保證不丟失日誌）
  - `true`：佇列滿時丟棄（適合即時系統）

### 5. NullSink（空輸出目標）

**位置**：`src/colog/null_sink.h`

```cpp
class NullSink : public ISink {
public:
    void write(std::string_view /*message*/) override {
        // 故意為空 - 丟棄所有輸出
    }

    void flush() override {
        // 無需刷新
    }
};
```

**用途**：
- **效能測試**：測量純日誌系統開銷，不包含 I/O 成本
- **開發除錯**：臨時禁用日誌輸出

---

## 🔒 執行緒安全設計

Phase2 的執行緒安全主要依賴：

### 1. 無鎖資料結構

- **LockFreeQueue**：使用原子操作和 CAS，無需互斥鎖
- **原子變數**：`std::atomic` 用於標誌和計數器

### 2. 記憶體順序保證

```cpp
// 生產者寫入
slot->sequence.store(pos + 1, std::memory_order_release);

// 消費者讀取
std::size_t seq = slot->sequence.load(std::memory_order_acquire);
```

**acquire-release 語義**：
- 生產者的 `release` 確保所有前面的寫入對消費者可見
- 消費者的 `acquire` 確保讀取後的操作不會重排到讀取之前
- 形成**同步點**，保證資料一致性

### 3. 條件變數同步

```cpp
std::mutex mutex_;
std::condition_variable cv_;
```

用於：
- 工作執行緒的等待和喚醒
- flush 請求的同步

---

## 📊 資料流範例

讓我們追蹤一次完整的非同步日誌呼叫：

```cpp
logger->info("Hello World");
```

1. **使用者呼叫** `logger->info("Hello World")`
2. **AsyncLogger::info()** 呼叫 `log(LogLevel::Info, "Hello World")`
3. **級別檢查**：如果 `level_ > Info`，直接返回
4. **建立 LogRecord**：立即捕獲時間戳
5. **建立 AsyncLogItem**：包含 record、formatter 和 sinks 的副本
6. **提交到佇列**：`AsyncBackend::submit()` → `queue_->try_push()`
   - 如果佇列滿且 `discard_on_full=false`，會自旋等待
   - 否則立即返回
7. **立即返回**：`log()` 方法返回，使用者程式碼繼續執行
8. **背景處理**（在工作執行緒中）：
   - `worker_loop()` 呼叫 `process_batch()`
   - `queue_->try_pop()` 取出項目
   - `formatter->format()` 格式化
   - `sink->write()` 寫入所有 Sink

---

## 🎯 設計模式總結

Phase2 使用了以下設計模式和技術：

| 模式/技術 | 應用位置 | 目的 |
|----------|---------|------|
| **單例模式** | AsyncBackend | 確保全域只有一個後端實例 |
| **生產者-消費者** | AsyncLogger → Queue → AsyncBackend | 解耦生產和消費 |
| **無鎖程式設計** | LockFreeQueue | 減少鎖競爭，提高效能 |
| **批次處理** | process_batch() | 提高吞吐量 |
| **RAII** | `unique_lock`、`unique_ptr` | 自動資源管理 |

---

## 🚀 使用範例

### 基本使用

```cpp
#include "colog/colog.h"

int main() {
    // 初始化非同步後端
    CoLog::AsyncConfig config;
    config.queue_size = 8192;
    config.batch_size = 256;
    config.flush_interval = std::chrono::milliseconds(100);
    CoLog::init_async(config);

    // 建立非同步日誌器
    auto logger = std::make_shared<CoLog::AsyncLogger>("async");
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    logger->add_sink(std::make_shared<CoLog::FileSink>("async.log"));

    // 記錄日誌（立即返回，不阻塞）
    logger->info("Application started");
    logger->warn("This is a warning");
    logger->error("An error occurred");

    // 等待所有日誌處理完成
    logger->flush_wait();

    // 優雅關閉
    CoLog::shutdown_async();

    return 0;
}
```

### 多執行緒使用

```cpp
void worker_thread(int id, CoLog::AsyncLoggerPtr logger) {
    for (int i = 0; i < 1000; ++i) {
        logger->info("Thread " + std::to_string(id) + 
                     " message " + std::to_string(i));
    }
}

int main() {
    CoLog::init_async();
    
    auto logger = std::make_shared<CoLog::AsyncLogger>("multi");
    logger->add_sink(std::make_shared<CoLog::FileSink>("multi.log"));

    // 啟動多個執行緒
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back(worker_thread, i, logger);
    }

    // 等待所有執行緒完成
    for (auto& t : threads) {
        t.join();
    }

    // 等待所有日誌處理完成
    logger->flush_wait();
    CoLog::shutdown_async();

    return 0;
}
```

### 效能測試

```cpp
void benchmark() {
    CoLog::init_async();
    
    auto logger = std::make_shared<CoLog::AsyncLogger>("bench");
    logger->add_sink(std::make_shared<CoLog::NullSink>());  // 只測量開銷

    constexpr int NUM_MESSAGES = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        logger->info("Message " + std::to_string(i));
    }
    
    auto enqueue_time = std::chrono::high_resolution_clock::now();
    logger->flush_wait();
    auto end = std::chrono::high_resolution_clock::now();
    
    // 計算吞吐量...
}
```

---

## ⚠️ Phase2 的限制與注意事項

### 優點
- ✅ **非阻塞**：`log()` 呼叫立即返回
- ✅ **高效能**：無鎖佇列減少鎖競爭
- ✅ **高吞吐量**：批次處理提高效率
- ✅ **執行緒安全**：多執行緒可以安全地同時寫入

### 限制
- ⚠️ **記憶體使用**：佇列會佔用記憶體（`queue_size * sizeof(AsyncLogItem)`）
- ⚠️ **背壓處理**：如果生產速度 > 消費速度，佇列會滿
- ⚠️ **關閉順序**：必須在應用程式關閉前呼叫 `shutdown_async()`
- ⚠️ **時間戳精度**：時間戳在 `log()` 呼叫時捕獲，而非實際寫入時

### 注意事項

**1. 初始化順序**
```cpp
// 必須在使用 AsyncLogger 之前初始化
CoLog::init_async();
auto logger = std::make_shared<CoLog::AsyncLogger>("...");
```

**2. 優雅關閉**
```cpp
// 應用程式關閉前必須呼叫
CoLog::shutdown_async();  // 會等待所有待處理日誌完成
```

**3. Formatter 和 Sink 的執行緒安全**
- Phase2 中，每個 `AsyncLogItem` 包含 formatter 和 sinks 的副本
- 但如果多個 Logger 共享同一個 Sink，該 Sink 必須是執行緒安全的
- Phase1 的 `ConsoleSink` 和 `FileSink` 已經有內建的互斥鎖保護

---

## 🔮 與 Phase1 的對比

| 特性 | Phase1（同步） | Phase2（非同步） |
|------|---------------|-----------------|
| **log() 阻塞** | ✅ 是 | ❌ 否 |
| **鎖使用** | `std::mutex` | 無鎖佇列 |
| **吞吐量** | 較低 | 高（批次處理） |
| **延遲** | 低（立即寫入） | 稍高（批次延遲） |
| **記憶體** | 低 | 較高（佇列緩衝） |
| **除錯難度** | 簡單 | 較複雜 |
| **適用場景** | 除錯、低頻日誌 | 生產環境、高頻日誌 |

---

## 📚 相關技術參考

### C++20 特性
- `std::atomic`：原子操作和記憶體順序
- `std::thread`：執行緒管理
- `std::condition_variable`：執行緒同步
- `std::memory_order`：記憶體順序控制

### 無鎖程式設計
- **CAS（Compare-And-Swap）**：`compare_exchange_weak/strong`
- **記憶體順序**：acquire-release 語義
- **偽共享**：快取行對齊

### 設計原則
- **生產者-消費者模式**：解耦生產和消費
- **批次處理**：平衡延遲和吞吐量
- **優雅關閉**：確保資料完整性

---

## 📝 總結

Phase2 實作了一個**高效能、非阻塞、執行緒安全**的非同步日誌系統。雖然實作複雜度較高，但它：

1. ✅ 大幅提高了吞吐量（特別是多執行緒環境）
2. ✅ 消除了 I/O 阻塞對主執行緒的影響
3. ✅ 展示了無鎖程式設計和原子操作的高階技術
4. ✅ 為生產環境提供了可靠的日誌解決方案

對於新手來說，Phase2 是學習**無鎖程式設計**、**記憶體順序**、**生產者-消費者模式**和**執行緒同步**的絕佳範例。

---

## 🔍 進階主題

### 為什麼不使用 C++20 協程？

雖然專案目標提到 C++20 Coroutines，但 Phase2 目前使用傳統的 `std::thread`。原因包括：

1. **成熟度**：`std::thread` 更成熟，跨平台支援更好
2. **除錯**：傳統執行緒更容易除錯和追蹤
3. **效能**：對於這種場景，執行緒和協程的效能差異不大

**未來可能的改進**：
- 使用 `std::jthread`（C++20）替代 `std::thread`，支援自動停止
- 探索使用協程實現更細粒度的控制流

### 無鎖佇列的效能考量

**為什麼無鎖佇列比有鎖佇列快？**
- **減少上下文切換**：無需等待鎖，減少執行緒切換
- **減少快取失效**：鎖會導致快取行失效
- **更好的擴展性**：多核心環境下效能更好

**但無鎖佇列也有代價**：
- **實現複雜**：需要仔細處理記憶體順序
- **CPU 使用**：自旋等待可能增加 CPU 使用率
- **除錯困難**：競態條件更難重現和除錯


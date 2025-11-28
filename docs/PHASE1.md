# Phase1: 同步日誌系統實作文件

## 📋 概述

Phase1 實作了 CoLog 的基礎同步日誌系統（Baseline Synchronous Logger）。這是一個**執行緒安全**、**模組化設計**的日誌庫，為後續的非同步日誌系統（Phase2）奠定了堅實的基礎。

### 完成的功能

- ✅ 完整的日誌記錄系統（Logger）
- ✅ 多種輸出目標（Sink）：控制台和檔案
- ✅ 靈活的格式化器（Formatter）：基於模式的文字格式化
- ✅ 日誌級別過濾（Trace, Debug, Info, Warn, Error, Critical）
- ✅ Logger 註冊表（Registry）：統一管理多個 Logger 實例
- ✅ 執行緒安全：使用互斥鎖保護共享資源
- ✅ C++20 特性：`std::source_location` 自動捕獲程式碼位置

---

## 🏗️ 架構設計

Phase1 採用了經典的**三層架構**設計，這是現代日誌庫（如 spdlog、log4j）的標準模式：

```
使用者程式碼
   ↓
Logger（日誌器）← 前端介面，提供 log()、info()、error() 等方法
   ↓
Formatter（格式化器）← 將日誌記錄轉換為字串
   ↓
Sink（輸出目標）← 實際寫入檔案或控制台
```

### 為什麼採用這種架構？

1. **關注點分離**：每個元件只負責一件事
2. **易於擴展**：可以輕鬆新增新的 Sink（如網路輸出）或 Formatter（如 JSON）
3. **可測試性**：每個元件都可以獨立測試
4. **符合開閉原則**：對擴展開放，對修改關閉

---

## 🔧 核心元件詳解

### 1. Logger（日誌器）

**位置**：`src/colog/logger.h` 和 `src/colog/logger.cpp`

Logger 是使用者直接使用的介面，負責：
- 接收日誌呼叫（`info()`, `error()` 等）
- 進行日誌級別過濾（早期返回，避免不必要的處理）
- 建立日誌記錄（LogRecord）
- 呼叫格式化器和 Sink

#### 關鍵技術點

**a) 早期級別過濾（Early Level Filtering）**

```cpp
void Logger::log(LogLevel level, const std::string& message, ...) {
    // 在取得鎖之前就檢查級別，避免不必要的開銷
    if (level < level_) {
        return;  // 直接返回，不進行任何格式化或寫入操作
    }
    // ... 後續處理
}
```

**為什麼這樣做？**
- 如果日誌級別低於設定的最小級別，直接返回
- **不需要取得鎖**，減少鎖競爭
- 這是效能優化的關鍵：避免為不需要的日誌做任何工作

**b) 執行緒安全保護**

```cpp
std::lock_guard<std::mutex> lock(mutex_);
```

`std::lock_guard` 是 **RAII（Resource Acquisition Is Initialization）** 模式的典型應用：
- 建構時自動取得鎖
- 解構時自動釋放鎖（即使發生異常）
- 確保異常安全（Exception Safety）

**c) C++20 `std::source_location`**

```cpp
void log(LogLevel level, const std::string& message,
         std::source_location loc = std::source_location::current());
```

這是 C++20 的新特性，可以自動捕獲：
- 檔案名稱（`loc.file_name()`）
- 函數名稱（`loc.function_name()`）
- 行號（`loc.line()`）
- 欄號（`loc.column()`）

**使用範例**：
```cpp
logger->info("Hello");  // 自動捕獲呼叫位置，無需手動傳遞 __FILE__、__LINE__
```

### 2. Formatter（格式化器）

**位置**：`src/colog/formatter.h` 和 `src/colog/pattern_formatter.h/cpp`

Formatter 負責將 `LogRecord` 結構轉換為字串。Phase1 實作了 `PatternFormatter`，輸出格式類似：
```
[2024-01-01 12:00:00.123] [INFO] [logger_name] message
```

#### 關鍵技術點

**a) 介面抽象（Interface Abstraction）**

```cpp
class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const LogRecord& record) = 0;
};
```

這是**策略模式（Strategy Pattern）**的應用：
- 定義統一的介面
- 不同的實作可以有不同的格式化方式（文字、JSON 等）
- Logger 不需要知道具體的格式化實作

**b) 時間戳格式化**

```cpp
auto time_t_val = std::chrono::system_clock::to_time_t(record.timestamp);
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
              record.timestamp.time_since_epoch()) % 1000;
```

使用 `std::chrono` 進行高精度時間處理：
- `system_clock`：系統時鐘，用於取得當前時間
- `duration_cast`：時間單位轉換
- 提取毫秒部分用於顯示

**c) 跨平台時間函數**

```cpp
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);  // Windows 安全版本
#else
    localtime_r(&time_t_val, &tm_buf);  // POSIX 執行緒安全版本
#endif
```

`localtime()` 不是執行緒安全的，所以：
- Windows 使用 `localtime_s()`（帶 `_s` 後綴的安全版本）
- Linux/macOS 使用 `localtime_r()`（`_r` 表示可重入/執行緒安全）

### 3. Sink（輸出目標）

**位置**：`src/colog/sink.h`、`console_sink.h/cpp`、`file_sink.h/cpp`

Sink 負責將格式化後的字串寫入目標位置。Phase1 實作了兩種 Sink：

#### ConsoleSink（控制台輸出）

```cpp
void ConsoleSink::write(std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << message;
}
```

**關鍵技術點**：
- 使用 `std::string_view` 而不是 `const std::string&`：避免不必要的字串複製
- 每個 Sink 有自己的互斥鎖：不同 Sink 可以並行寫入（雖然 Phase1 是同步的）

#### FileSink（檔案輸出）

```cpp
class FileSink : public ISink {
private:
    std::ofstream file_;
    mutable std::mutex mutex_;  // mutable 允許在 const 方法中使用
};
```

**關鍵技術點**：

**a) RAII 資源管理**

```cpp
FileSink::~FileSink() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}
```

解構函數確保：
- 檔案在物件銷毀時自動刷新和關閉
- 即使忘記呼叫 `flush()`，檔案也會被正確關閉

**b) `mutable` 關鍵字**

```cpp
bool is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);  // mutex_ 是 mutable 的
    return file_.is_open();
}
```

`mutable` 允許在 `const` 方法中修改成員變數：
- `is_open()` 是 `const` 方法（不改變物件邏輯狀態）
- 但需要取得鎖（改變互斥鎖狀態）
- `mutable` 讓這成為可能

**c) 檔案開啟模式**

```cpp
std::ios::openmode mode = append 
    ? (std::ios::out | std::ios::app)   // 追加模式
    : std::ios::out;                     // 覆蓋模式
```

使用位元標誌（Bit Flags）組合檔案開啟選項。

### 4. Registry（註冊表）

**位置**：`src/colog/registry.h` 和 `src/colog/registry.cpp`

Registry 負責管理所有的 Logger 實例，採用**單例模式（Singleton Pattern）**。

#### 關鍵技術點

**a) 單例模式實作**

```cpp
Registry& Registry::instance() {
    static Registry instance;  // C++11 保證執行緒安全的靜態區域變數初始化
    return instance;
}
```

這是**Meyers' Singleton**的實作方式：
- C++11 標準保證靜態區域變數的初始化是執行緒安全的
- 延遲初始化：只有在第一次呼叫時才建立實例
- 自動解構：程式結束時自動銷毀

**b) 懶載入 Logger**

```cpp
LoggerPtr Registry::get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;  // 已存在，直接返回
    }
    
    // 不存在，建立新的
    auto logger = std::make_shared<Logger>(name);
    loggers_[name] = logger;
    return logger;
}
```

**設計優勢**：
- 按需建立：只有在使用時才建立 Logger
- 複用實例：相同名稱的 Logger 只建立一次
- 統一管理：所有 Logger 都在一個地方管理

**c) 智慧指標的使用**

```cpp
using LoggerPtr = std::shared_ptr<Logger>;
```

使用 `std::shared_ptr` 的原因：
- **自動記憶體管理**：不需要手動 `delete`
- **共享所有權**：多個地方可以持有同一個 Logger 的參考
- **執行緒安全**：`shared_ptr` 的參考計數操作是原子的（但物件本身不是執行緒安全的）

### 5. LogRecord（日誌記錄）

**位置**：`src/colog/record.h`

`LogRecord` 是一個簡單的資料結構（POD-like），包含一條日誌的所有資訊：

```cpp
struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
    std::string_view logger_name;
    std::source_location location;
};
```

**設計考慮**：
- 使用 `std::string_view` 儲存 logger 名稱：避免不必要的字串複製
- 時間戳使用 `time_point`：高精度時間表示
- 包含 `source_location`：自動記錄程式碼位置

### 6. LogLevel（日誌級別）

**位置**：`src/colog/level.h`

```cpp
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};
```

**使用 `enum class` 而不是 `enum`**：
- **型別安全**：不能隱式轉換為整數
- **作用域**：級別名稱不會污染全域命名空間
- **可比較**：可以安全地使用 `<`、`>` 進行比較

---

## 🔒 執行緒安全設計

Phase1 是**同步日誌系統**，意味著每次呼叫 `log()` 都會阻塞直到日誌寫入完成。執行緒安全透過以下方式保證：

### 1. Logger 級別的鎖

```cpp
class Logger {
private:
    std::mutex mutex_;  // 保護 sinks_ 和 formatter_
};
```

保護：
- Sink 列表的修改
- Formatter 的設定
- 日誌寫入過程

### 2. Sink 級別的鎖

每個 Sink 都有自己的鎖：

```cpp
class ConsoleSink {
private:
    std::mutex mutex_;  // 保護 std::cout 的寫入
};
```

**為什麼每個 Sink 都需要鎖？**
- 多個 Logger 可能共享同一個 Sink
- 需要保證寫入操作的原子性

### 3. Registry 的鎖

```cpp
class Registry {
private:
    std::mutex mutex_;  // 保護 loggers_ 對應表
};
```

保護 Logger 的建立、查找和刪除操作。

---

## 📊 資料流範例

讓我們追蹤一次完整的日誌呼叫：

```cpp
logger->info("Hello World");
```

1. **使用者呼叫** `logger->info("Hello World")`
2. **Logger::info()** 呼叫 `log(LogLevel::Info, "Hello World")`
3. **級別檢查**：如果 `level_ > Info`，直接返回
4. **建立 LogRecord**：
   ```cpp
   LogRecord record(LogLevel::Info, "Hello World", "main", 
                    std::source_location::current());
   ```
5. **取得鎖**：`std::lock_guard<std::mutex> lock(mutex_)`
6. **格式化**：`formatter_->format(record)` → `"[2024-01-01 12:00:00] [INFO] [main] Hello World\n"`
7. **寫入所有 Sink**：
   - `console_sink->write(formatted)` → 輸出到控制台
   - `file_sink->write(formatted)` → 寫入檔案
8. **釋放鎖**：`lock_guard` 解構，自動釋放鎖
9. **返回**：函數返回，使用者程式碼繼續執行

---

## 🎯 設計模式總結

Phase1 使用了多種經典設計模式：

| 模式 | 應用位置 | 目的 |
|------|---------|------|
| **策略模式** | Formatter、Sink 介面 | 允許執行時選擇不同的格式化/輸出策略 |
| **單例模式** | Registry | 確保全域只有一個註冊表實例 |
| **RAII** | `lock_guard`、檔案流 | 自動資源管理，異常安全 |
| **工廠模式** | Registry::get() | 統一建立和管理 Logger 實例 |

---

## 🚀 使用範例

### 基本使用

```cpp
#include "colog/colog.h"

int main() {
    // 取得或建立名為 "main" 的 Logger
    auto logger = CoLog::get_logger("main");
    
    // 新增控制台輸出
    logger->add_sink(std::make_shared<CoLog::ConsoleSink>());
    
    // 新增檔案輸出
    logger->add_sink(std::make_shared<CoLog::FileSink>("app.log"));
    
    // 設定日誌級別（只記錄 Info 及以上級別）
    logger->set_level(CoLog::LogLevel::Info);
    
    // 記錄日誌
    logger->info("Application started");
    logger->warn("This is a warning");
    logger->error("An error occurred");
    
    // 確保所有日誌都已寫入
    logger->flush();
    
    return 0;
}
```

### 使用預設 Logger

```cpp
// 取得預設 Logger（如果不存在會自動建立）
auto default_logger = CoLog::get_default_logger();
default_logger->info("Using default logger");
```

---

## ⚠️ Phase1 的限制

Phase1 是**同步日誌系統**，有以下特點：

### 優點
- ✅ **簡單可靠**：邏輯清晰，易於理解和除錯
- ✅ **執行緒安全**：使用互斥鎖保證資料一致性
- ✅ **異常安全**：使用 RAII 確保資源正確釋放

### 缺點
- ❌ **效能瓶頸**：每次 `log()` 呼叫都會阻塞，直到寫入完成
- ❌ **鎖競爭**：多執行緒環境下，鎖競爭可能成為瓶頸
- ❌ **I/O 阻塞**：檔案寫入是同步的，會阻塞呼叫執行緒

### 為什麼 Phase1 是必要的？

Phase1 為後續的非同步日誌系統（Phase2）提供了：
1. **穩定的基礎架構**：Logger-Formatter-Sink 三層架構
2. **可測試的基準**：可以對比同步和非同步的效能差異
3. **除錯工具**：同步日誌更容易除錯（立即看到輸出）

---

## 🔮 下一步：Phase2 預覽

Phase2 將引入：
- **非同步日誌**：`log()` 呼叫立即返回，不阻塞
- **C++20 協程**：使用協程處理後台日誌寫入
- **無鎖佇列**：使用 lock-free 資料結構減少鎖競爭
- **批次寫入**：一次寫入多條日誌，提高吞吐量

---

## 📚 相關技術參考

### C++20 特性
- `std::source_location`：自動捕獲程式碼位置
- `std::string_view`：輕量級字串視圖，避免複製

### 標準庫元件
- `std::mutex` 和 `std::lock_guard`：執行緒同步
- `std::shared_ptr`：智慧指標，自動記憶體管理
- `std::chrono`：高精度時間處理
- `std::ofstream`：檔案流操作

### 設計原則
- **RAII**：資源取得即初始化
- **單一職責原則**：每個類別只做一件事
- **開閉原則**：對擴展開放，對修改關閉
- **依賴倒置**：依賴抽象介面，而不是具體實作

---

## 📝 總結

Phase1 實作了一個**功能完整、執行緒安全、易於擴展**的同步日誌系統。雖然效能不是最優的，但它：

1. ✅ 建立了清晰的架構基礎
2. ✅ 展示了現代 C++ 的最佳實踐
3. ✅ 為 Phase2 的非同步優化打下了堅實基礎
4. ✅ 提供了可靠的除錯和開發工具

對於新手來說，Phase1 是學習現代 C++ 設計模式、執行緒安全和資源管理的絕佳範例。

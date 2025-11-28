#ifndef COLOG_H
#define COLOG_H

// CoLog - A high-performance C++20 logging library

// Core types
#include "level.h"
#include "record.h"

// Formatter
#include "formatter.h"
#include "pattern_formatter.h"

// Sinks
#include "sink.h"
#include "console_sink.h"
#include "file_sink.h"
#include "null_sink.h"

// Synchronous Logger
#include "logger.h"

// Async Logger
#include "async_logger.h"
#include "async/async_backend.h"

// Registry
#include "registry.h"

#endif  // COLOG_H


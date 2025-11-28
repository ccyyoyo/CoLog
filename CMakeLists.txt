cmake_minimum_required(VERSION 3.20)

project(CoLog 
    VERSION 0.1.0 
    LANGUAGES CXX
)

# C++20 Standard Requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Global compiler options
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Enable Testing
enable_testing()

# --- CoLog Core Library ---
set(COLOG_SOURCES
    src/colog/pattern_formatter.cpp
    src/colog/file_sink.cpp
    src/colog/console_sink.cpp
    src/colog/logger.cpp
    src/colog/registry.cpp
    # Async components
    src/colog/async/async_backend.cpp
    src/colog/async_logger.cpp
)

add_library(colog STATIC ${COLOG_SOURCES})
target_include_directories(colog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Thread support for async backend
find_package(Threads REQUIRED)
target_link_libraries(colog PUBLIC Threads::Threads)

# --- Demo Executable ---
add_executable(colog_demo src/main.cpp)
target_link_libraries(colog_demo PRIVATE colog)

message(STATUS "CoLog configured for ${CMAKE_SYSTEM_NAME}")

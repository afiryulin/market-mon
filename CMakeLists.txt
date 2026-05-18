cmake_minimum_required(VERSION 3.28)
project(market-monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# IntelliSense and clang-tidy support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------
# Global options
# ---------------------------------------------------
option(ENABLE_TESTS "Enable tests" OFF)

# ---------------------------------------------------
# Install dependencies
# ---------------------------------------------------
include(cmake/InstallDeps.cmake)

configure_file(
    ${PROJECT_SOURCE_DIR}/common/Config.h.in
    ${PROJECT_BINARY_DIR}/common/Config.h
)

add_subdirectory(proto)
add_subdirectory(server)
add_subdirectory(client)
if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()

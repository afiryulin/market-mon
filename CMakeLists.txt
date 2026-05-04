cmake_minimum_required(VERSION 3.28)
project(market-monitor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# IntelliSense and clang-tidy support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------
# CPM (package manager)
# ---------------------------------------------------
include(cmake/CPM.cmake)

# ---------------------------------------------------
# Global options
# ---------------------------------------------------
option(ENABLE_TESTS "Enable tests" OFF)

# ---------------------------------------------------
# Dependencies
# ---------------------------------------------------


# gRPC
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)

list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake/grpc" "/usr/local/lib/cmake/protobuf")

# set(gRPC_AS_DEPENDENCY ON)
find_package(protobuf CONFIG QUIET)
if(NOT protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

find_package(gRPC CONFIG REQUIRED)

message(STATUS "GRPC: " gRPC_FOUND)

if(gRPC_FOUND)
    message(STATUS "SUCCESS: System gRPC found!")

    if(NOT TARGET grpc_cpp_plugin)
        find_program(GRPC_CPP_PLUGIN_EXE grpc_cpp_plugin)
        add_executable(grpc_cpp_plugin IMPORTED)
        set_property(TARGET grpc_cpp_plugin PROPERTY IMPORTED_LOCATION ${GRPC_CPP_PLUGIN_EXE})
    endif()
else()

    message(STATUS "gRPC and/or Protobuf NOT found, downloading...")

    set(GRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)

    CPMAddPackage(
        NAME gRPC
        GITHUB_REPOSITORY grpc/grpc
        VERSION 1.80.0
        GIT_TAG v1.80.0
        SYSTEM YES
        OPTIONS
            "gRPC_INSTALL OFF"
            "gRPC_BUILD_TESTS OFF"
            "gRPC_ABSL_PROVIDER module"
            "gRPC_PROTOBUF_PROVIDER module"
    )
endif()


# fmt
CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 11.0.2
    OPTIONS
        "FMT_TEST OFF"
)

# spdlog
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.17.0
    OPTIONS
        "SPDLOG_BUILD_TESTS OFF"
        "SPDLOG_FMT_EXTERNAL ON"
)

# yaml-cpp
# CPMAddPackage(
#     NAME yaml-cpp
#     GITHUB_REPOSITORY jbeder/yaml-cpp
#     GIT_TAG yaml-cpp-0.9.0
#     OPTIONS
#         "yaml-cpp_BUILD_TESTS OFF"
# )

# ---------------------------------------------------
# Tests
# ---------------------------------------------------
if(ENABLE_TESTS)
    enable_testing()

    CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.17.0
        OPTIONS
            "INSTALL_GTEST OFF"
    )

endif()


add_subdirectory(proto)
add_subdirectory(server)
add_subdirectory(client)

# ---------------------------------------------------
# CPM (package manager) installation and dependencies
# ---------------------------------------------------

# CPM
set(CPM_LOCAL_PATH "${CMAKE_SOURCE_DIR}/cmake/CPM.cmake")

if(NOT EXISTS "${CPM_LOCAL_PATH}")
    message(STATUS "CMake CPM NOT found, downloading...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
        "${CPM_LOCAL_PATH}"
        STATUS download_status
    )
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(FATAL_ERROR "Download error CPM.cmake: ${download_status}")
    endif()
endif()

include("${CPM_LOCAL_PATH}")

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
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt NOT found, downloading...")
    CPMAddPackage(
        NAME fmt
        GITHUB_REPOSITORY fmtlib/fmt
        GIT_TAG 11.0.2
        OPTIONS
            "FMT_TEST OFF"
    )
endif()


# spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        VERSION 1.17.0
        OPTIONS
            "SPDLOG_BUILD_TESTS OFF"
            "SPDLOG_FMT_EXTERNAL ON"
    )
endif()

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

    find_package(googletest QUIET)
    if(NOT googletest_FOUND)
        message(STATUS "googletest NOT found, downloading...")
        CPMAddPackage(
            NAME googletest
            GITHUB_REPOSITORY google/googletest
            VERSION 1.17.0
            OPTIONS
                "INSTALL_GTEST OFF"
        )
    endif()
endif()

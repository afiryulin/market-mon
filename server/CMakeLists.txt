add_subdirectory(core)

set(SERVER_EXECUTABLE market_server)

add_executable(${SERVER_EXECUTABLE}
    src/main.cpp
)

target_include_directories(${SERVER_EXECUTABLE} PRIVATE "${PROJECT_BINARY_DIR}")


target_link_libraries(
    ${SERVER_EXECUTABLE}
    PRIVATE
        market_server_core
        spdlog::spdlog
)

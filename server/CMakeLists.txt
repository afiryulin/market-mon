set(TARGET_NAME market_server)
add_executable(${TARGET_NAME}
    src/main.cpp
    src/AsyncMarketServer.cpp
    src/SubscribePriceCallData.cpp
)

configure_file(
    ${PROJECT_SOURCE_DIR}/common/Config.h.in
    ${PROJECT_BINARY_DIR}/common/Config.h
)

target_include_directories(${TARGET_NAME} PUBLIC "${PROJECT_BINARY_DIR}")

target_link_libraries(${TARGET_NAME}
PRIVATE
    market_proto
    grpc++
    spdlog::spdlog
)
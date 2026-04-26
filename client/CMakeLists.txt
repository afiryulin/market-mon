add_executable(market_client
     src/main.cpp
     src/PriceClient.cpp
     src/TradeClient.cpp
 )

configure_file(${PROJECT_SOURCE_DIR}/common/Config.h.in common/Config.h)
target_include_directories(market_client PUBLIC "${PROJECT_BINARY_DIR}")

target_link_libraries(market_client
PRIVATE
    market_proto
    gRPC::grpc++
    spdlog::spdlog
)
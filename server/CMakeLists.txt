add_executable(market_server
    src/main.cpp
    src/AsyncMarketServer.cpp
    src/SubscribePriceCallData.cpp
)

target_link_libraries(    market_server
PRIVATE
    market_proto
    grpc++
    spdlog::spdlog
)
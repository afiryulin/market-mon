set(SERVER_CORE market_server_core)

add_library(${SERVER_CORE}
    STATIC
        src/PriceGenerator.cpp
        src/SubscriberManager.cpp
        src/AsyncMarketServer.cpp
        src/SubscribePriceCallData.cpp
        src/GetPriceCallData.cpp
        src/TradeCallData.cpp
        src/MatchingEngine.cpp
)

target_include_directories(${SERVER_CORE}
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_BINARY_DIR}/proto"
)

target_link_libraries(${SERVER_CORE}
    INTERFACE
        market_proto
    PRIVATE
        gRPC::grpc++
        spdlog::spdlog
)

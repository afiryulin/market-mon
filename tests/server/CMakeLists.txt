set(TEST_TARGET matching_engine_integration_test)

add_executable(${TEST_TARGET}
    TradeStreamIntegrationTest.cpp)

target_include_directories(${TEST_TARGET}
    PUBLIC
        ${PROJECT_BINARY_DIR})

target_link_libraries(${TEST_TARGET}
    PRIVATE
        market_proto
        market_server_core
        GTest::gtest
        GTest::gtest_main
        gRPC::grpc++
        spdlog::spdlog
)

add_test(
    NAME matching_engine_integration_test
    COMMAND matching_engine_integration_test
)

add_executable(mpsc_queue_test
    MPSCQueueTest.cpp
)

target_link_libraries(mpsc_queue_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        market_server_core
)

target_include_directories(mpsc_queue_test
    PUBLIC
        ${PROJECT_BINARY_DIR})

add_test(
    NAME mpsc_queue_test
    COMMAND mpsc_queue_test
)

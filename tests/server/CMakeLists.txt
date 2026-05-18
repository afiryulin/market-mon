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

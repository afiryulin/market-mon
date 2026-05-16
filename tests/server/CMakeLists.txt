add_executable(matching_engine_test)

target_sources(matching_engine_test
    PRIVATE
        MatchingEngineTest.cpp
)

target_link_libraries(matching_engine_test
    PRIVATE
        market_proto
        gRPC::grpc++
        spdlog::spdlog
)

target_include_directories(matching_engine_test PUBLIC ${PROJECT_BINARY_DIR})

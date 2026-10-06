add_library(itch_parser
    STATIC
        src/ItchParser.c
)

target_include_directories(itch_parser
    PUBLIC
        include
)



if(ENABLE_TESTS)
    add_executable(itch_parser_tests
        tests/ItchParserTest.cpp
    )

    target_link_libraries(itch_parser_tests
        PRIVATE
            itch_parser
            gtest
            gtest_main
    )

    add_test(NAME itch_parser_tests COMMAND itch_parser_tests)
endif()

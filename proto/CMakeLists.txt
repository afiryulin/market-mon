add_library(market_proto STATIC)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/market/v1/market.proto
)

target_sources(market_proto PRIVATE ${PROTO_SOURCES})

target_link_libraries(market_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

target_include_directories(market_proto
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

foreach(proto_source ${PROTO_SOURCES})
    get_filename_component(proto_path "${proto_source}" ABSOLUTE)
    get_filename_component(proto_name "${proto_source}" NAME_WE)
    get_filename_component(proto_dir "${proto_source}" DIRECTORY)

    file(RELATIVE_PATH rel_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${proto_dir}")

    set(pb_src "${CMAKE_CURRENT_BINARY_DIR}/${rel_dir}/${proto_name}.pb.cc")
    set(grpc_src "${CMAKE_CURRENT_BINARY_DIR}/${rel_dir}/${proto_name}.grpc.pb.cc")

    list(APPEND PROTO_GENERATED_FILES "${pb_cc}" "${grpc_cc}")

    add_custom_command(
        OUTPUT ${pb_src} ${grpc_src}
        COMMAND protobuf::protoc
        ARGS
            --grpc_out ${CMAKE_CURRENT_BINARY_DIR}
            --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
            -I ${CMAKE_CURRENT_SOURCE_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
            ${proto_source}
        DEPENDS ${proto_source}
        COMMENT "Generating C++ code from ${proto_source}"
        VERBATIM
    )
    target_sources(market_proto PRIVATE ${pb_src} ${grpc_src})
endforeach()
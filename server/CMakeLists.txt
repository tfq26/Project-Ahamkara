add_executable(ahamkara_server
    src/dedicated_server_main.cpp
)

target_compile_features(ahamkara_server PRIVATE cxx_std_20)

target_link_libraries(ahamkara_server
    PRIVATE
        ae_core
        ae_network
        ae_runtime
        ahamkara_game
)

set_target_properties(ahamkara_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server
)


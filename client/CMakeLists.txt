add_executable(ahamkara_client
    src/client_config.cpp
    src/controller_bindings.cpp
    src/main.cpp
    src/debug_client.cpp
    src/headless_clients.cpp
    src/local_play.cpp
    src/window_input_provider.cpp
    src/audio_player.cpp
)

target_compile_features(ahamkara_client PRIVATE cxx_std_20)

target_include_directories(ahamkara_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ahamkara_client
    PRIVATE
        ae_network
        ae_runtime
        ae_render
        ahamkara_game
        miniaudio_lib
)

set_target_properties(ahamkara_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client
)

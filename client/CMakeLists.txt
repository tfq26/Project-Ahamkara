add_executable(ahamkara_client
    src/client_config.cpp
    src/client_frame_pipeline.cpp
    src/client_menu_state.cpp
    src/controller_bindings.cpp
    src/main.cpp
    src/debug_client.cpp
    src/debug_frontend_runtime.cpp
    src/debug_render_runtime.cpp
    src/debug_scene_bridge.cpp
    src/debug_ui_controller.cpp
    src/headless_clients.cpp
    src/local_play.cpp
    src/threaded_local_runtime.cpp
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
        ae_ui
        ae_audio
        ae_input
        ahamkara_game
        miniaudio_lib
)

set_target_properties(ahamkara_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client
)

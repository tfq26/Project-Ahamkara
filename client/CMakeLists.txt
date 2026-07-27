# Reusable client runtime (everything except the executable entry point).
# Demos (samples/flashback) and tests link this so they ride the real engine,
# game, and client stack and inherit engine improvements automatically.
#
# NOTE: ahamkara_game is intentionally PUBLIC here because several public
# headers (playtest_harness.h, local_play.h, debug_scene_bridge.h, ...) expose
# ahamkara::game types.  Once those headers are decoupled (e.g. via forward
# declarations or a game-agnostic snapshot API) this can become PRIVATE,
# which is the upstream goal for the Ahamkara package boundary.
add_library(ahamkara_client_lib STATIC
    src/client_config.cpp
    src/client_frame_pipeline.cpp
    src/client_menu_state.cpp
    src/controller_bindings.cpp
    src/debug_client.cpp
    src/debug_frontend_runtime.cpp
    src/debug_inspector.cpp
    src/debug_render_runtime.cpp
    src/debug_scene_bridge.cpp
    src/debug_ui_controller.cpp
    src/headless_clients.cpp
    src/local_play.cpp
    src/weapon_animation_controller.cpp
    src/threaded_local_runtime.cpp
    src/window_input_provider.cpp
    src/audio_player.cpp
    src/weapon_presentation.cpp
    src/playtest_harness.cpp
)
target_compile_features(ahamkara_client_lib PUBLIC cxx_std_20)

target_include_directories(ahamkara_client_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ahamkara_client_lib
    PUBLIC
        ae_network
        ae_runtime
        ae_render
        ae_ui
        ae_audio
        ae_input
        ahamkara_game
        ae_animation
        miniaudio_lib
)

add_executable(ahamkara_client
    src/main.cpp
)

target_compile_features(ahamkara_client PRIVATE cxx_std_20)

target_link_libraries(ahamkara_client
    PRIVATE
        ahamkara_client_lib
)

set_target_properties(ahamkara_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client
)

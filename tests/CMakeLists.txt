find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(ahamkara_smoke_tests
    src/network_smoke_tests.cpp
    src/camera_smoke_tests.cpp
    src/local_play_tests.cpp
    ../client/src/local_play.cpp
)

add_executable(ahamkara_playtest_harness_tests
    src/playtest_harness_tests.cpp
    ../client/src/local_play.cpp
    ../client/src/playtest_harness.cpp
)

add_executable(ahamkara_world_tests
    src/world_tests.cpp
)

add_executable(ahamkara_player_entity_tests
    src/player_entity_tests.cpp
)

add_executable(ahamkara_movement_tests
    src/movement_tests.cpp
)

add_executable(ahamkara_player_movement_controller_tests
    src/player_movement_controller_tests.cpp
)

if(TARGET ae_platform)
    add_executable(ahamkara_window_input_provider_tests
        src/window_input_provider_tests.cpp
        ../client/src/window_input_provider.cpp
    )

    target_compile_features(ahamkara_window_input_provider_tests PRIVATE cxx_std_20)

    target_include_directories(ahamkara_window_input_provider_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine/platform/include
    )

    target_link_libraries(ahamkara_window_input_provider_tests
        PRIVATE
            ae_platform
            ahamkara_game
    )

    add_test(NAME ahamkara_window_input_provider_tests COMMAND ahamkara_window_input_provider_tests)
endif()
target_compile_features(ahamkara_smoke_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_playtest_harness_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_world_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_player_entity_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_movement_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_player_movement_controller_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_smoke_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(ahamkara_smoke_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_include_directories(ahamkara_playtest_harness_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/render/include
)

target_link_libraries(ahamkara_playtest_harness_tests
    PRIVATE
        ae_network
        ae_runtime
        ahamkara_game
)
target_link_libraries(ahamkara_smoke_tests
    PRIVATE
        ae_network
        ae_runtime
        ahamkara_game
)

# ae_render only exists in GUI (non-headless) configs; link it when available.
if(TARGET ae_render)
    target_link_libraries(ahamkara_smoke_tests PRIVATE ae_render)
    target_link_libraries(ahamkara_playtest_harness_tests PRIVATE ae_render)
endif()

target_link_libraries(ahamkara_world_tests
    PRIVATE
        ae_runtime
        ahamkara_game
)

target_link_libraries(ahamkara_player_entity_tests
    PRIVATE
        ae_runtime
        ahamkara_game
)

target_link_libraries(ahamkara_movement_tests
    PRIVATE
        ahamkara_game
)

target_link_libraries(ahamkara_player_movement_controller_tests
    PRIVATE
        ahamkara_game
)

add_test(NAME ahamkara_smoke_tests COMMAND ahamkara_smoke_tests)
add_test(NAME ahamkara_playtest_harness_tests COMMAND ahamkara_playtest_harness_tests)
add_test(NAME ahamkara_world_tests COMMAND ahamkara_world_tests)
add_test(NAME ahamkara_player_entity_tests COMMAND ahamkara_player_entity_tests)
add_test(NAME ahamkara_movement_tests COMMAND ahamkara_movement_tests)
add_test(NAME ahamkara_player_movement_controller_tests COMMAND ahamkara_player_movement_controller_tests)
add_executable(ahamkara_collision_tests
    src/collision_tests.cpp
)

target_compile_features(ahamkara_collision_tests PRIVATE cxx_std_20)

target_link_libraries(ahamkara_collision_tests
    PRIVATE
        ae_collision
)

add_test(NAME ahamkara_collision_tests COMMAND ahamkara_collision_tests)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../game/include/ahamkara/game/gameplay_types.h"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../game/include/ahamkara/game/components.h"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../game/src/gameplay_types.cpp")
    add_executable(ahamkara_gameplay_tests
        src/gameplay_tests.cpp
    )

    target_compile_features(ahamkara_gameplay_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_gameplay_tests
        PRIVATE
            ahamkara_game
    )

    add_test(NAME ahamkara_gameplay_tests COMMAND ahamkara_gameplay_tests)
endif()

add_executable(ahamkara_session_tests
    src/session_runtime_tests.cpp
)

target_compile_features(ahamkara_session_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(ahamkara_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)
target_include_directories(ahamkara_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../wish/include)
target_include_directories(ahamkara_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_include_directories(ahamkara_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/network/include)

target_link_libraries(ahamkara_session_tests
    PRIVATE
        ae_network
        wish_engine
)

add_test(NAME ahamkara_session_tests COMMAND ahamkara_session_tests)

add_executable(ahamkara_utility_tests
    src/utility_tests.cpp
)

add_executable(ahamkara_logging_tests
    src/logging_tests.cpp
)

target_compile_features(ahamkara_utility_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_utility_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_utility_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_utility_tests COMMAND ahamkara_utility_tests)

target_compile_features(ahamkara_logging_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_logging_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_logging_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_logging_tests COMMAND ahamkara_logging_tests)

add_executable(ahamkara_core_tests
    src/core_tests.cpp
)

target_compile_features(ahamkara_core_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_core_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_core_tests COMMAND ahamkara_core_tests)

add_executable(ahamkara_weapon_runtime_tests
    src/weapon_runtime_tests.cpp
)

target_compile_features(ahamkara_weapon_runtime_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_weapon_runtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)

target_link_libraries(ahamkara_weapon_runtime_tests
    PRIVATE
        ahamkara_game
)

add_test(NAME ahamkara_weapon_runtime_tests COMMAND ahamkara_weapon_runtime_tests)

add_executable(ahamkara_weapon_loader_tests
    src/weapon_loader_tests.cpp
)

target_compile_features(ahamkara_weapon_loader_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_weapon_loader_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)

target_link_libraries(ahamkara_weapon_loader_tests
    PRIVATE
        ahamkara_game
)

add_test(NAME ahamkara_weapon_loader_tests COMMAND ahamkara_weapon_loader_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Weapon animation controller tests (client-dependent, not available headless)
if(TARGET ahamkara_client_lib)
    add_executable(ahamkara_weapon_animation_controller_tests
        src/weapon_animation_controller_tests.cpp
    )
    target_compile_features(ahamkara_weapon_animation_controller_tests PRIVATE cxx_std_20)
    target_include_directories(ahamkara_weapon_animation_controller_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../game/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/animation/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/skeleton/include
    )
    target_link_libraries(ahamkara_weapon_animation_controller_tests
        PRIVATE
            ahamkara_client_lib
    )
    add_test(NAME ahamkara_weapon_animation_controller_tests
        COMMAND ahamkara_weapon_animation_controller_tests)
endif()

add_test(
    NAME agent_runner_python_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/agent-runner/tests/test_agent_runner.py
)

add_test(
    NAME lint_runner_python_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/lint/tests/test_lint_runner.py
)

add_executable(ahamkara_nakama_bridge_tests
    src/nakama_bridge_tests.cpp
)

target_compile_features(ahamkara_nakama_bridge_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_nakama_bridge_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_nakama_bridge_tests
    PRIVATE
        wish_engine
)

add_test(NAME ahamkara_nakama_bridge_tests COMMAND ahamkara_nakama_bridge_tests)

if(TARGET ae_render)
    add_executable(ahamkara_asset_pipeline_tests
        src/asset_pipeline_tests.cpp
    )

    target_compile_features(ahamkara_asset_pipeline_tests PRIVATE cxx_std_20)
    add_dependencies(ahamkara_asset_pipeline_tests ahamkara_asset_importer)

    target_compile_definitions(ahamkara_asset_pipeline_tests
        PRIVATE
            AHAMKARA_ASSET_IMPORTER_PATH="$<TARGET_FILE:ahamkara_asset_importer>"
    )

    target_link_libraries(ahamkara_asset_pipeline_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_asset_pipeline_tests COMMAND ahamkara_asset_pipeline_tests)
endif()

if(TARGET ae_render)
    add_executable(ahamkara_level_render_tests
        src/level_render_tests.cpp
    )

    target_compile_features(ahamkara_level_render_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_level_render_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_level_render_tests COMMAND ahamkara_level_render_tests)

    add_executable(ahamkara_lod_batching_tests
        src/lod_batching_tests.cpp
    )

    target_compile_features(ahamkara_lod_batching_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_lod_batching_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_lod_batching_tests COMMAND ahamkara_lod_batching_tests)

    add_executable(ahamkara_ibl_tests
        src/ibl_tests.cpp
    )

    target_compile_features(ahamkara_ibl_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_ibl_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_ibl_tests COMMAND ahamkara_ibl_tests)

    add_executable(ahamkara_ssao_tests
        src/ssao_tests.cpp
    )

    target_compile_features(ahamkara_ssao_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_ssao_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_ssao_tests COMMAND ahamkara_ssao_tests)

    add_executable(ahamkara_color_grading_tests
        src/color_grading_tests.cpp
    )

    target_compile_features(ahamkara_color_grading_tests PRIVATE cxx_std_20)

    target_link_libraries(ahamkara_color_grading_tests
        PRIVATE
            ae_render
    )

    add_test(NAME ahamkara_color_grading_tests COMMAND ahamkara_color_grading_tests)
endif()

add_executable(ahamkara_reliable_channel_tests
    src/reliable_channel_tests.cpp
)

target_compile_features(ahamkara_reliable_channel_tests PRIVATE cxx_std_20)

target_link_libraries(ahamkara_reliable_channel_tests
    PRIVATE
        ae_network
)

add_test(NAME ahamkara_reliable_channel_tests COMMAND ahamkara_reliable_channel_tests)

add_executable(ahamkara_interpolation_buffer_tests
    src/interpolation_buffer_tests.cpp
)

target_compile_features(ahamkara_interpolation_buffer_tests PRIVATE cxx_std_20)

target_include_directories(ahamkara_interpolation_buffer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/network/include
)

target_link_libraries(ahamkara_interpolation_buffer_tests
    PRIVATE
        ae_network
)

add_test(NAME ahamkara_interpolation_buffer_tests COMMAND ahamkara_interpolation_buffer_tests)

add_executable(ahamkara_console_tests
    src/console_tests.cpp
    ../engine/core/src/console.cpp
)

target_compile_features(ahamkara_console_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_console_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_console_tests PRIVATE ae_core)
add_test(NAME ahamkara_console_tests COMMAND ahamkara_console_tests)

add_executable(ahamkara_file_watcher_tests
    src/file_watcher_tests.cpp
    ../engine/core/src/file_watcher.cpp
    ../engine/core/src/log.cpp
)

target_compile_features(ahamkara_file_watcher_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_file_watcher_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_file_watcher_tests PRIVATE ae_core)
add_test(NAME ahamkara_file_watcher_tests COMMAND ahamkara_file_watcher_tests)

add_executable(ahamkara_nav_grid_tests
    src/nav_grid_tests.cpp
)

target_compile_features(ahamkara_nav_grid_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_nav_grid_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)

add_test(NAME ahamkara_nav_grid_tests COMMAND ahamkara_nav_grid_tests)

add_executable(ahamkara_ai_combatant_tests
    src/ai_combatant_tests.cpp
)

target_compile_features(ahamkara_ai_combatant_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_ai_combatant_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)
target_include_directories(ahamkara_ai_combatant_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_ai_combatant_tests PRIVATE ahamkara_game)

add_test(NAME ahamkara_ai_combatant_tests COMMAND ahamkara_ai_combatant_tests)

add_executable(ahamkara_encounter_scripting_tests
    src/encounter_scripting_tests.cpp
)

target_compile_features(ahamkara_encounter_scripting_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_encounter_scripting_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../game/include)
target_include_directories(
    ahamkara_encounter_scripting_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
)
target_link_libraries(ahamkara_encounter_scripting_tests PRIVATE ahamkara_game)

add_test(NAME ahamkara_encounter_scripting_tests COMMAND ahamkara_encounter_scripting_tests)

add_executable(ahamkara_streaming_residency_tests
    src/streaming_residency_tests.cpp
)

target_compile_features(ahamkara_streaming_residency_tests PRIVATE cxx_std_20)

target_include_directories(ahamkara_streaming_residency_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
)

target_link_libraries(ahamkara_streaming_residency_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_streaming_residency_tests COMMAND ahamkara_streaming_residency_tests)

# --- Destination Metadata ---
add_executable(ahamkara_destination_metadata_tests
    src/destination_metadata_tests.cpp
)

target_compile_features(ahamkara_destination_metadata_tests PRIVATE cxx_std_20)

target_include_directories(ahamkara_destination_metadata_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../game/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
)

target_link_libraries(ahamkara_destination_metadata_tests
    PRIVATE
        ahamkara_game
)

add_test(NAME ahamkara_destination_metadata_tests COMMAND ahamkara_destination_metadata_tests)

# --- Profiling Tests ---
add_executable(ahamkara_profiling_tests
    src/profiling_tests.cpp
)

target_compile_features(ahamkara_profiling_tests PRIVATE cxx_std_20)

target_include_directories(ahamkara_profiling_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include
)

target_link_libraries(ahamkara_profiling_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_profiling_tests COMMAND ahamkara_profiling_tests)

add_executable(ahamkara_telemetry_tests
    src/telemetry_tests.cpp
    ../engine/core/src/telemetry.cpp
)

target_compile_features(ahamkara_telemetry_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_telemetry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_telemetry_tests PRIVATE ae_core)
add_test(NAME ahamkara_telemetry_tests COMMAND ahamkara_telemetry_tests)

add_executable(ahamkara_crash_handler_tests
    src/crash_handler_tests.cpp
    ../engine/core/src/crash_handler.cpp
    ../engine/core/src/time.cpp
    ../engine/core/src/log.cpp
)

target_compile_features(ahamkara_crash_handler_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_crash_handler_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_crash_handler_tests PRIVATE ae_core)
add_test(NAME ahamkara_crash_handler_tests COMMAND ahamkara_crash_handler_tests)

add_executable(ahamkara_diagnostics_tests
    src/diagnostics_tests.cpp
    ../engine/core/src/diagnostics.cpp
    ../engine/core/src/crash_handler.cpp
    ../engine/core/src/time.cpp
    ../engine/core/src/log.cpp
)

target_compile_features(ahamkara_diagnostics_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_diagnostics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
target_link_libraries(ahamkara_diagnostics_tests PRIVATE ae_core)
add_test(NAME ahamkara_diagnostics_tests COMMAND ahamkara_diagnostics_tests)

add_executable(ahamkara_runtime_boundary_tests
    src/runtime_boundary_tests.cpp
)

target_compile_features(ahamkara_runtime_boundary_tests PRIVATE cxx_std_20)
target_compile_definitions(ahamkara_runtime_boundary_tests PRIVATE
    $<$<BOOL:${AHAMKARA_BUILD_CLIENT}>:AHAMKARA_BUILD_CLIENT=$<BOOL:${AHAMKARA_BUILD_CLIENT}>>
    $<$<BOOL:${AHAMKARA_BUILD_SERVER}>:AHAMKARA_BUILD_SERVER=$<BOOL:${AHAMKARA_BUILD_SERVER}>>
)
add_test(NAME ahamkara_runtime_boundary_tests COMMAND ahamkara_runtime_boundary_tests)

# CI regression coverage: these exercise the actual generated build/install
# graph instead of matching target names in source text.
if(TARGET ahamkara_controller_mapper)
    add_test(
        NAME ahamkara_controller_mapper_build_regression
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --config $<CONFIG>
                --target ahamkara_controller_mapper
    )
endif()

if(TARGET ahamkara_server)
    add_test(
        NAME ahamkara_server_build_regression
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --config $<CONFIG>
                --target ahamkara_server
    )
endif()

add_test(
    NAME ahamkara_install_rules_smoke
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
            --config $<CONFIG>
            --prefix ${CMAKE_BINARY_DIR}/test-install
)

add_executable(ahamkara_animation_headless_tests
    src/animation_headless_tests.cpp
)
target_link_libraries(ahamkara_animation_headless_tests
    PRIVATE
        ae_animation
        ae_skeleton
        ae_core
)
add_test(NAME ahamkara_animation_headless_tests COMMAND ahamkara_animation_headless_tests)

# Out-of-tree package consumer smoke (engine package boundary).
# Builds a tiny external project against an installed Ahamkara package prefix.
add_test(
    NAME ahamkara_out_of_tree_package_consumer
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_BINARY_DIR}
        -DCONSUMER_SRC=${CMAKE_SOURCE_DIR}/tests/out_of_tree_consumer
        -P ${CMAKE_SOURCE_DIR}/cmake/OutOfTreeConsumerTest.cmake
)

add_executable(ahamkara_error_identity_tests
    src/error_identity_tests.cpp
)
target_link_libraries(ahamkara_error_identity_tests
    PRIVATE
        ae_core
)
add_test(NAME ahamkara_error_identity_tests COMMAND ahamkara_error_identity_tests)

add_executable(ahamkara_game_module_host_tests
    src/game_module_host_tests.cpp
)
target_link_libraries(ahamkara_game_module_host_tests
    PRIVATE
        ae_runtime
        ae_core
)
add_test(NAME ahamkara_game_module_host_tests COMMAND ahamkara_game_module_host_tests)

add_executable(ahamkara_wish_error_tests
    src/wish_error_tests.cpp
)
target_compile_features(ahamkara_wish_error_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_wish_error_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(ahamkara_wish_error_tests
    PRIVATE
        wish_engine
)
add_test(NAME ahamkara_wish_error_tests COMMAND ahamkara_wish_error_tests)

add_executable(ahamkara_metrics_tests
    src/metrics_tests.cpp
)
target_compile_features(ahamkara_metrics_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_metrics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(ahamkara_metrics_tests
    PRIVATE
        wish_engine
)
add_test(NAME ahamkara_metrics_tests COMMAND ahamkara_metrics_tests)

add_executable(ahamkara_rewind_tests
    src/rewind_tests.cpp
)
target_compile_features(ahamkara_rewind_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_rewind_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../game/include
)
target_link_libraries(ahamkara_rewind_tests
    PRIVATE
        ahamkara_game
)
add_test(NAME ahamkara_rewind_tests COMMAND ahamkara_rewind_tests)

add_executable(ahamkara_connection_lifecycle_tests
    src/connection_lifecycle_tests.cpp
)
target_compile_features(ahamkara_connection_lifecycle_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_connection_lifecycle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(ahamkara_connection_lifecycle_tests
    PRIVATE
        ae_network
)
add_test(NAME ahamkara_connection_lifecycle_tests COMMAND ahamkara_connection_lifecycle_tests)

add_executable(ahamkara_jitter_buffer_tests
    src/jitter_buffer_tests.cpp
)
target_compile_features(ahamkara_jitter_buffer_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_jitter_buffer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(ahamkara_jitter_buffer_tests
    PRIVATE
        ae_network
)
add_test(NAME ahamkara_jitter_buffer_tests COMMAND ahamkara_jitter_buffer_tests)

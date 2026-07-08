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

target_link_libraries(ahamkara_session_tests
    PRIVATE
        ae_network
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

add_test(
    NAME agent_runner_python_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/agent-runner/tests/test_agent_runner.py
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
target_include_directories(ahamkara_encounter_scripting_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core/include)
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

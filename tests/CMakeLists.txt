add_executable(ahamkara_smoke_tests
    src/network_smoke_tests.cpp
    src/camera_smoke_tests.cpp
    src/local_play_tests.cpp
    ../client/src/local_play.cpp
)

add_executable(ahamkara_world_tests
    src/world_tests.cpp
)

add_executable(ahamkara_movement_tests
    src/movement_tests.cpp
)

target_compile_features(ahamkara_smoke_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_world_tests PRIVATE cxx_std_20)
target_compile_features(ahamkara_movement_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_smoke_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_include_directories(ahamkara_smoke_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_link_libraries(ahamkara_smoke_tests
    PRIVATE
        ae_network
        ae_runtime
        ahamkara_game
        ae_render
)

target_link_libraries(ahamkara_world_tests
    PRIVATE
        ae_runtime
        ahamkara_game
)

target_link_libraries(ahamkara_movement_tests
    PRIVATE
        ahamkara_game
)

add_test(NAME ahamkara_smoke_tests COMMAND ahamkara_smoke_tests)
add_test(NAME ahamkara_world_tests COMMAND ahamkara_world_tests)
add_test(NAME ahamkara_movement_tests COMMAND ahamkara_movement_tests)

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

target_compile_features(ahamkara_utility_tests PRIVATE cxx_std_20)
target_include_directories(ahamkara_utility_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(ahamkara_utility_tests
    PRIVATE
        ae_core
)

add_test(NAME ahamkara_utility_tests COMMAND ahamkara_utility_tests)

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
endif()

add_library(ahamkara_game STATIC
    src/game_module.cpp
    src/movement.cpp
    src/maps/javelin4.cpp
    src/world.cpp
    src/worlds/debug_javelin4_world.cpp
    src/world_jolt_bridge.cpp
    src/world_dummy_sim.cpp
    src/world_projectile.cpp
    src/world_camera.cpp
    src/client_prediction.cpp
    src/item_registry.cpp
    src/deathmatch_mode.cpp
    src/activities/deathmatch_activity.cpp
    src/activities/horde_activity.cpp
    src/activities/social_hub_activity.cpp
    ../engine/render/src/compiled_level.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gameplay_types.cpp")
    target_sources(ahamkara_game PRIVATE src/gameplay_types.cpp)
endif()

target_compile_features(ahamkara_game PUBLIC cxx_std_20)

target_include_directories(ahamkara_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/render/include
)

target_link_libraries(ahamkara_game
    PUBLIC
        ae_core
        Jolt
        EnTT
        wish_engine
    PRIVATE
        ae_render
        ae_physics
)

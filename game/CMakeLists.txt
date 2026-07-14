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
    src/player_movement_controller.cpp
    src/client_prediction.cpp
    src/player.cpp
    src/weapon_loader.cpp
    src/weapon_runtime.cpp
    src/item_registry.cpp
    src/map.cpp
    src/deathmatch_mode.cpp
    src/activities/deathmatch_activity.cpp
    src/activities/horde_activity.cpp
    src/activities/social_hub_activity.cpp
    src/ai_combatant.cpp
    src/encounter_scripting.cpp
    ../engine/render/src/compiled_level.cpp
    src/animation_adapter.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gameplay_types.cpp")
    target_sources(ahamkara_game PRIVATE src/gameplay_types.cpp)
endif()

target_compile_features(ahamkara_game PUBLIC cxx_std_20)

target_include_directories(ahamkara_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/render/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/animation/include
)

target_link_libraries(ahamkara_game
    PUBLIC
        ae_core
        Jolt
        EnTT
        wish_engine
    PRIVATE
        ae_physics
        ae_animation
)

# ae_render is only built in GUI (non-headless) configs.  ahamkara_game compiles
# the GL-free compiled_level.cpp itself and references no other render symbols,
# so link the render library only when the target exists (keeps headless linking).
if(TARGET ae_render)
    target_link_libraries(ahamkara_game PRIVATE ae_render)
endif()

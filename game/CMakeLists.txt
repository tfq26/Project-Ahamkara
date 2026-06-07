add_library(ahamkara_game STATIC
    src/game_module.cpp
    src/movement.cpp
    src/world.cpp
    src/world_jolt_bridge.cpp
    src/world_dummy_sim.cpp
    src/world_projectile.cpp
    src/world_camera.cpp
    src/client_prediction.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gameplay_types.cpp")
    target_sources(ahamkara_game PRIVATE src/gameplay_types.cpp)
endif()

target_compile_features(ahamkara_game PUBLIC cxx_std_20)

target_include_directories(ahamkara_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ahamkara_game
    PUBLIC
        ae_core
        Jolt
        EnTT
)

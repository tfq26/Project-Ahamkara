add_library(ae_animation STATIC
    src/types.cpp
    src/state_machine.cpp
    src/animation_graph.cpp
    src/animation_driver.cpp
    src/character_weapon.cpp
    src/aim_recoil.cpp
    src/ik.cpp
    src/debug.cpp
    src/animation_render_bridge.cpp
    src/clip_player.cpp
)

target_compile_features(ae_animation PUBLIC cxx_std_20)

target_include_directories(ae_animation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ae_animation
    PUBLIC
        ae_skeleton
        ae_core
)

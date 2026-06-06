add_library(ahamkara_game STATIC
    src/game_module.cpp
    src/movement.cpp
    src/world.cpp
)

target_compile_features(ahamkara_game PUBLIC cxx_std_20)

target_include_directories(ahamkara_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ahamkara_game
    PUBLIC
        ae_core
        Jolt
)

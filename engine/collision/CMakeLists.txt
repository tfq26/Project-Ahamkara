add_library(ae_collision STATIC
    src/collision_world.cpp
    src/trace.cpp
    src/debug.cpp
)

target_compile_features(ae_collision PUBLIC cxx_std_20)

target_include_directories(ae_collision
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ae_collision
    PUBLIC
        ae_core
        Jolt
)

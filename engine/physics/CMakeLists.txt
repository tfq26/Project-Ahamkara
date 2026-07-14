add_library(ae_physics STATIC
    src/physics_world.cpp
)

target_compile_features(ae_physics PUBLIC cxx_std_20)

target_include_directories(ae_physics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ae_physics
    PUBLIC
        ae_core
        Jolt
)

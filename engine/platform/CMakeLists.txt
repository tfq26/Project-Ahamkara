add_library(ae_platform STATIC
    src/window_glfw.cpp
)

target_compile_features(ae_platform PUBLIC cxx_std_20)

target_include_directories(ae_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(glfw3 REQUIRED)

target_link_libraries(ae_platform
    PUBLIC
        ae_core
        glfw
)

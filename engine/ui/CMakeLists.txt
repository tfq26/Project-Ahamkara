add_library(ae_ui STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
    src/ahamkara_ui.cpp
    src/menu_system.cpp
    src/hud_element.cpp
    src/hud_system.cpp
    src/settings_catalog.cpp
)

target_compile_features(ae_ui PUBLIC cxx_std_20)

target_include_directories(ae_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ae_ui
    PUBLIC
        ae_render
        ae_platform
    PRIVATE
        OpenGL::GL
        glfw
)

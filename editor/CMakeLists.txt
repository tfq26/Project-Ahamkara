# ── Ahamkara Editor ───────────────────────────────────────────────────────────
#
# Build target for the in-engine editor window.
# Reuses ae_platform (GLFW) for window creation and ae_ui (ImGui) for
# the dockable editor interface.

add_library(ahamkara_editor_lib STATIC
    src/editor_window.cpp
)

target_compile_features(ahamkara_editor_lib PUBLIC cxx_std_20)

target_include_directories(ahamkara_editor_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ahamkara_editor_lib
    PUBLIC
        ae_core
        ae_platform
        ae_ui
    PRIVATE
        glfw
)

# ── Editor executable ─────────────────────────────────────────────────────────
add_executable(ahamkara_editor
    src/main.cpp
)

target_compile_features(ahamkara_editor PRIVATE cxx_std_20)

target_link_libraries(ahamkara_editor
    PRIVATE
        ahamkara_editor_lib
)

set_target_properties(ahamkara_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/editor
)

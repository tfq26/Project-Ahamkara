add_library(ae_render STATIC
    src/compiled_level.cpp
    src/compiled_material.cpp
    src/compiled_mesh.cpp
    src/compiled_texture.cpp
    src/debug_renderer.cpp
    src/debug_renderer_effects.cpp
    src/debug_renderer_gpu.cpp
    src/debug_renderer_hud.cpp
    src/debug_renderer_metrics.cpp
    src/font_atlas.cpp
    src/frustum.cpp
    src/gl_compat.cpp
    src/gltf_loader.cpp
    src/humanoid_mesh.cpp
    src/level_render.cpp
    src/map_geometry.cpp
    src/pbr_renderer.cpp
    src/render_backend_opengl.cpp
    src/shadow_pass.cpp
    src/skeletal_animation.cpp
    src/platform/mac_text_rasterizer.cpp
    src/text_rasterizer.cpp
    src/weapon_model_cache.cpp
)

target_compile_features(ae_render PUBLIC cxx_std_20)

target_include_directories(ae_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(APPLE)
    target_compile_definitions(ae_render PRIVATE GL_SILENCE_DEPRECATION)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
endif()

target_link_libraries(ae_render
    PUBLIC
        ae_core
        ae_platform
    PRIVATE
        ae_animation
        OpenGL::GL
        glfw
        $<$<PLATFORM_ID:Darwin>:${COREFOUNDATION_FRAMEWORK}>
        $<$<PLATFORM_ID:Darwin>:${CORETEXT_FRAMEWORK}>
        $<$<PLATFORM_ID:Darwin>:${COREGRAPHICS_FRAMEWORK}>
)

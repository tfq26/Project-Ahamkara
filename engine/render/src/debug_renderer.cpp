#include "ae/render/debug_renderer.h"

#include "ae/core/log.h"
#include "ae/render/font_atlas.h"
#include "ae/render/humanoid_mesh.h"
#include "ae/render/map_geometry.h"
#include "ae/render/render_backend.h"
#include "ae/render/skeletal_animation.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GL_GLEXT_PROTOTYPES
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
// GL_TIME_ELAPSED is from GL_EXT_timer_query / GL_ARB_timer_query
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_SAMPLES_PASSED
#define GL_SAMPLES_PASSED 0x8914
#endif
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "debug_renderer_internal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gl_compat.h"

namespace ae::render {

// ============================================================================
// Shared font atlas (used by draw_text in this TU and in overlay files)
// ============================================================================

FontAtlas& shared_ui_font_atlas() {
    static FontAtlas atlas;
    static bool attempted_init = false;
    if (!attempted_init) {
        attempted_init = true;
        atlas.initialize_default();
    }
    return atlas;
}

// ============================================================================
// Math helpers (Vec3, LocalMat4, perspective, look_at)
// ============================================================================

const float kPi = 3.14159265358979323846F;

// LocalMat4 struct definition lives in debug_renderer_internal.h.
// Implementation helpers follow.

Vec3 subtract(Vec3 lhs, Vec3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 cross(Vec3 lhs, Vec3 rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float dot(Vec3 lhs, Vec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 normalize(Vec3 value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.00001F) {
        return {};
    }

    return {value.x / length, value.y / length, value.z / length};
}

LocalMat4 perspective(float vertical_fov_radians, float aspect_ratio, float near_plane, float far_plane) {
    const float f = 1.0F / std::tan(vertical_fov_radians * 0.5F);

    LocalMat4 result {};
    result.values[0] = f / aspect_ratio;
    result.values[5] = f;
    result.values[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.values[11] = -1.0F;
    result.values[14] = (2.0F * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

LocalMat4 look_at(Vec3 eye, Vec3 target, Vec3 world_up) {
    const Vec3 forward = normalize(subtract(target, eye));
    const Vec3 side = normalize(cross(world_up, forward));
    const Vec3 up = cross(forward, side);

    LocalMat4 result {};
    result.values[0] = side.x;
    result.values[4] = side.y;
    result.values[8] = side.z;

    result.values[1] = up.x;
    result.values[5] = up.y;
    result.values[9] = up.z;

    result.values[2] = -forward.x;
    result.values[6] = -forward.y;
    result.values[10] = -forward.z;

    result.values[12] = -dot(side, eye);
    result.values[13] = -dot(up, eye);
    result.values[14] = dot(forward, eye);
    result.values[15] = 1.0F;
    return result;
}

// ============================================================================
// Primitive drawing helpers
// ============================================================================

void set_color(float red, float green, float blue) {
    glColor3f(red, green, blue);
}

void draw_line(Vec3 from, Vec3 to) {
    glVertex3f(from.x, from.y, from.z);
    glVertex3f(to.x, to.y, to.z);
}

void draw_ground_grid(int half_extent, float spacing, float brightness) {
    // Minimum floor so grid lines stay visible even at night.
    float b = 0.20F + 0.80F * brightness;
    glLineWidth(1.0F);
    glBegin(GL_LINES);
    for (int index = -half_extent; index <= half_extent; ++index) {
        float g = (index == 0 ? 0.32F : 0.18F) * b;
        set_color(g, g, g);

        const float offset = static_cast<float>(index) * spacing;
        const float extent = static_cast<float>(half_extent) * spacing;
        draw_line({-extent, 0.0F, offset}, {extent, 0.0F, offset});
        draw_line({offset, 0.0F, -extent}, {offset, 0.0F, extent});
    }
    glEnd();
}

void draw_axes() {
    glLineWidth(3.0F);
    glBegin(GL_LINES);
    set_color(1.0F, 0.12F, 0.12F);
    draw_line({}, {2.5F, 0.0F, 0.0F});
    set_color(0.12F, 1.0F, 0.12F);
    draw_line({}, {0.0F, 2.5F, 0.0F});
    set_color(0.2F, 0.42F, 1.0F);
    draw_line({}, {0.0F, 0.0F, 2.5F});
    glEnd();
    glLineWidth(1.0F);
}

// --- Box primitive (6 quad faces) ---
void draw_box(Vec3 min, Vec3 max) {
    glBegin(GL_QUADS);
    // +Z face
    glNormal3f(0,0,1);
    glVertex3f(min.x, min.y, max.z); glVertex3f(max.x, min.y, max.z); glVertex3f(max.x, max.y, max.z); glVertex3f(min.x, max.y, max.z);
    // -Z face
    glNormal3f(0,0,-1);
    glVertex3f(max.x, min.y, min.z); glVertex3f(min.x, min.y, min.z); glVertex3f(min.x, max.y, min.z); glVertex3f(max.x, max.y, min.z);
    // +X face
    glNormal3f(1,0,0);
    glVertex3f(max.x, min.y, max.z); glVertex3f(max.x, min.y, min.z); glVertex3f(max.x, max.y, min.z); glVertex3f(max.x, max.y, max.z);
    // -X face
    glNormal3f(-1,0,0);
    glVertex3f(min.x, min.y, min.z); glVertex3f(min.x, min.y, max.z); glVertex3f(min.x, max.y, max.z); glVertex3f(min.x, max.y, min.z);
    // +Y face
    glNormal3f(0,1,0);
    glVertex3f(min.x, max.y, max.z); glVertex3f(max.x, max.y, max.z); glVertex3f(max.x, max.y, min.z); glVertex3f(min.x, max.y, min.z);
    // -Y face
    glNormal3f(0,-1,0);
    glVertex3f(min.x, min.y, min.z); glVertex3f(max.x, min.y, min.z); glVertex3f(max.x, min.y, max.z); glVertex3f(min.x, min.y, max.z);
    glEnd();
}

void draw_player_marker(Vec3 position, float height, float yaw) {
    const float cos_yaw = std::cos(yaw);
    const float sin_yaw = std::sin(yaw);
    auto rotate_xz = [&](float x, float z) -> std::pair<float, float> {
        return {x * cos_yaw - z * sin_yaw, x * sin_yaw + z * cos_yaw};
    };

    const float foot_y = position.y;
    const float hips_y = position.y + height * 0.45F;
    const float shoulders_y = position.y + height * 0.75F;
    const float head_center_y = position.y + height * 0.87F;
    const float body_half_width = 0.1F;
    const float body_half_depth = 0.08F;
    const float head_radius = 0.07F;

    // Foot box — small position reference cube
    {
        const float hs = 0.06F;
        const float top = foot_y + 0.06F;
        const float bx = position.x;
        const float bz = position.z;

        glBegin(GL_QUADS);
        // front
        glVertex3f(bx - hs, foot_y, bz + hs); glVertex3f(bx + hs, foot_y, bz + hs);
        glVertex3f(bx + hs, top, bz + hs); glVertex3f(bx - hs, top, bz + hs);
        // back
        glVertex3f(bx + hs, foot_y, bz - hs); glVertex3f(bx - hs, foot_y, bz - hs);
        glVertex3f(bx - hs, top, bz - hs); glVertex3f(bx + hs, top, bz - hs);
        // right
        glVertex3f(bx + hs, foot_y, bz + hs); glVertex3f(bx + hs, foot_y, bz - hs);
        glVertex3f(bx + hs, top, bz - hs); glVertex3f(bx + hs, top, bz + hs);
        // left
        glVertex3f(bx - hs, foot_y, bz - hs); glVertex3f(bx - hs, foot_y, bz + hs);
        glVertex3f(bx - hs, top, bz + hs); glVertex3f(bx - hs, top, bz - hs);
        // top
        glVertex3f(bx - hs, top, bz + hs); glVertex3f(bx + hs, top, bz + hs);
        glVertex3f(bx + hs, top, bz - hs); glVertex3f(bx - hs, top, bz - hs);
        // bottom
        glVertex3f(bx - hs, foot_y, bz - hs); glVertex3f(bx + hs, foot_y, bz - hs);
        glVertex3f(bx + hs, foot_y, bz + hs); glVertex3f(bx - hs, foot_y, bz + hs);
        glEnd();
    }

    // Legs (two lines from hips to feet)
    {
        const float leg_offset = 0.06F;
        const auto [lx, lz] = rotate_xz(-body_half_width * 0.5F, 0.0F);
        const auto [rx, rz] = rotate_xz( body_half_width * 0.5F, 0.0F);
        glLineWidth(2.0F);
        glBegin(GL_LINES);
        glVertex3f(position.x + lx, hips_y, position.z + lz);
        glVertex3f(position.x + lx, foot_y, position.z + lz);
        glVertex3f(position.x + rx, hips_y, position.z + rz);
        glVertex3f(position.x + rx, foot_y, position.z + rz);
        glEnd();
        glLineWidth(1.0F);
    }

    // Torso box
    {
        const float bx = position.x;
        const float bz = position.z;
        const float hw = body_half_width;
        const float hd = body_half_depth;

        auto [fl_x, fl_z] = rotate_xz(-hw, -hd);
        auto [fr_x, fr_z] = rotate_xz( hw, -hd);
        auto [bl_x, bl_z] = rotate_xz(-hw,  hd);
        auto [br_x, br_z] = rotate_xz( hw,  hd);

        glBegin(GL_QUADS);
        // Front
        glVertex3f(bx + fl_x, hips_y, bz + fl_z);
        glVertex3f(bx + fr_x, hips_y, bz + fr_z);
        glVertex3f(bx + fr_x, shoulders_y, bz + fr_z);
        glVertex3f(bx + fl_x, shoulders_y, bz + fl_z);
        // Back
        glVertex3f(bx + br_x, hips_y, bz + br_z);
        glVertex3f(bx + bl_x, hips_y, bz + bl_z);
        glVertex3f(bx + bl_x, shoulders_y, bz + bl_z);
        glVertex3f(bx + br_x, shoulders_y, bz + br_z);
        // Right
        glVertex3f(bx + fr_x, hips_y, bz + fr_z);
        glVertex3f(bx + br_x, hips_y, bz + br_z);
        glVertex3f(bx + br_x, shoulders_y, bz + br_z);
        glVertex3f(bx + fr_x, shoulders_y, bz + fr_z);
        // Left
        glVertex3f(bx + bl_x, hips_y, bz + bl_z);
        glVertex3f(bx + fl_x, hips_y, bz + fl_z);
        glVertex3f(bx + fl_x, shoulders_y, bz + fl_z);
        glVertex3f(bx + bl_x, shoulders_y, bz + bl_z);
        glEnd();
    }

    // Neck
    {
        const auto [nf_x, nf_z] = rotate_xz(0.0F, -0.03F);
        const auto [nb_x, nb_z] = rotate_xz(0.0F,  0.03F);
        const auto [nl_x, nl_z] = rotate_xz(-0.03F, 0.0F);
        const auto [nr_x, nr_z] = rotate_xz( 0.03F, 0.0F);
        glBegin(GL_QUADS);
        glVertex3f(position.x + nf_x, shoulders_y, position.z + nf_z);
        glVertex3f(position.x + nb_x, shoulders_y, position.z + nb_z);
        glVertex3f(position.x + nb_x, head_center_y - head_radius, position.z + nb_z);
        glVertex3f(position.x + nf_x, head_center_y - head_radius, position.z + nf_z);
        glEnd();
    }

    // Arms
    {
        const float shoulder_off_x = body_half_width + 0.02F;
        const float arm_length = height * 0.30F;
        const float hand_drop = height * 0.15F;
        const float hand_side = 0.05F;

        const auto [lsx, lsz] = rotate_xz(-shoulder_off_x, 0.0F);
        const auto [rsx, rsz] = rotate_xz( shoulder_off_x, 0.0F);
        const auto [lex, lez] = rotate_xz(-shoulder_off_x - hand_side, hand_drop);
        const auto [rex, rez] = rotate_xz( shoulder_off_x + hand_side, hand_drop);

        glLineWidth(2.0F);
        glBegin(GL_LINES);
        // Left arm
        glVertex3f(position.x + lsx, shoulders_y, position.z + lsz);
        glVertex3f(position.x + lex, shoulders_y - hand_drop, position.z + lez);
        // Right arm
        glVertex3f(position.x + rsx, shoulders_y, position.z + rsz);
        glVertex3f(position.x + rex, shoulders_y - hand_drop, position.z + rez);
        glEnd();
        glLineWidth(1.0F);
    }

    // Head circle
    {
        const float cx = position.x;
        const float cy = head_center_y;
        const float cz = position.z;
        const float r = head_radius;

        auto [fx, fz] = rotate_xz(0.0F, -r);
        auto [bx2, bz2] = rotate_xz(0.0F,  r);
        auto [lx2, lz2] = rotate_xz(-r, 0.0F);
        auto [rx2, rz2] = rotate_xz( r, 0.0F);

        const Vec3 top    = {cx, cy + r, cz};
        const Vec3 bottom = {cx, cy - r, cz};
        const Vec3 front2 = {cx + fx, cy, cz + fz};
        const Vec3 back2  = {cx + bx2, cy, cz + bz2};
        const Vec3 left2  = {cx + lx2, cy, cz + lz2};
        const Vec3 right2 = {cx + rx2, cy, cz + rz2};

        glLineWidth(2.0F);
        glBegin(GL_LINES);
        draw_line(top, front2); draw_line(top, back2);
        draw_line(top, left2);  draw_line(top, right2);
        draw_line(bottom, front2); draw_line(bottom, back2);
        draw_line(bottom, left2);  draw_line(bottom, right2);
        draw_line(front2, left2); draw_line(left2, back2);
        draw_line(back2, right2); draw_line(right2, front2);
        glEnd();
        glLineWidth(1.0F);
    }
}

// ============================================================================
// Bitmap font glyph table (fallback when FontAtlas is unavailable)
// ============================================================================

const std::array<std::uint8_t, 7>* glyph_for_char(char character) {
    static const std::array<std::uint8_t, 7> space {{0, 0, 0, 0, 0, 0, 0}};
    static const std::array<std::uint8_t, 7> colon {{0, 4, 4, 0, 4, 4, 0}};
    static const std::array<std::uint8_t, 7> dot {{0, 0, 0, 0, 0, 6, 6}};
    static const std::array<std::uint8_t, 7> slash {{1, 1, 2, 4, 8, 16, 16}};
    static const std::array<std::uint8_t, 7> percent {{17, 18, 4, 8, 19, 17, 0}};
    static const std::array<std::uint8_t, 7> dash {{0, 0, 0, 31, 0, 0, 0}};
    static const std::array<std::uint8_t, 7> zero {{14, 17, 19, 21, 25, 17, 14}};
    static const std::array<std::uint8_t, 7> one {{4, 12, 4, 4, 4, 4, 14}};
    static const std::array<std::uint8_t, 7> two {{14, 17, 1, 2, 4, 8, 31}};
    static const std::array<std::uint8_t, 7> three {{30, 1, 1, 14, 1, 1, 30}};
    static const std::array<std::uint8_t, 7> four {{2, 6, 10, 18, 31, 2, 2}};
    static const std::array<std::uint8_t, 7> five {{31, 16, 16, 30, 1, 1, 30}};
    static const std::array<std::uint8_t, 7> six {{14, 16, 16, 30, 17, 17, 14}};
    static const std::array<std::uint8_t, 7> seven {{31, 1, 2, 4, 8, 8, 8}};
    static const std::array<std::uint8_t, 7> eight {{14, 17, 17, 14, 17, 17, 14}};
    static const std::array<std::uint8_t, 7> nine {{14, 17, 17, 15, 1, 1, 14}};
    static const std::array<std::uint8_t, 7> a {{14, 17, 17, 31, 17, 17, 17}};
    static const std::array<std::uint8_t, 7> b {{30, 17, 17, 30, 17, 17, 30}};
    static const std::array<std::uint8_t, 7> c {{14, 17, 16, 16, 16, 17, 14}};
    static const std::array<std::uint8_t, 7> d {{30, 17, 17, 17, 17, 17, 30}};
    static const std::array<std::uint8_t, 7> e {{31, 16, 16, 30, 16, 16, 31}};
    static const std::array<std::uint8_t, 7> f {{31, 16, 16, 30, 16, 16, 16}};
    static const std::array<std::uint8_t, 7> g {{14, 17, 16, 23, 17, 17, 15}};
    static const std::array<std::uint8_t, 7> h {{17, 17, 17, 31, 17, 17, 17}};
    static const std::array<std::uint8_t, 7> i {{14, 4, 4, 4, 4, 4, 14}};
    static const std::array<std::uint8_t, 7> j {{1, 1, 1, 1, 17, 17, 14}};
    static const std::array<std::uint8_t, 7> k {{17, 18, 20, 24, 20, 18, 17}};
    static const std::array<std::uint8_t, 7> l {{16, 16, 16, 16, 16, 16, 31}};
    static const std::array<std::uint8_t, 7> m {{17, 27, 21, 17, 17, 17, 17}};
    static const std::array<std::uint8_t, 7> n {{17, 25, 21, 19, 17, 17, 17}};
    static const std::array<std::uint8_t, 7> o {{14, 17, 17, 17, 17, 17, 14}};
    static const std::array<std::uint8_t, 7> p {{30, 17, 17, 30, 16, 16, 16}};
    static const std::array<std::uint8_t, 7> q {{14, 17, 17, 17, 21, 18, 13}};
    static const std::array<std::uint8_t, 7> r {{30, 17, 17, 30, 20, 18, 17}};
    static const std::array<std::uint8_t, 7> s {{15, 16, 16, 14, 1, 1, 30}};
    static const std::array<std::uint8_t, 7> t {{31, 4, 4, 4, 4, 4, 4}};
    static const std::array<std::uint8_t, 7> u {{17, 17, 17, 17, 17, 17, 14}};
    static const std::array<std::uint8_t, 7> v {{17, 17, 17, 17, 17, 10, 4}};
    static const std::array<std::uint8_t, 7> w {{17, 17, 17, 21, 21, 21, 10}};
    static const std::array<std::uint8_t, 7> x {{17, 17, 10, 4, 10, 17, 17}};
    static const std::array<std::uint8_t, 7> y {{17, 17, 10, 4, 4, 4, 4}};
    static const std::array<std::uint8_t, 7> z {{31, 1, 2, 4, 8, 16, 31}};

    switch (character) {
        case ' ': return &space;
        case ':': return &colon;
        case '.': return &dot;
        case '/': return &slash;
        case '%': return &percent;
        case '-': return &dash;
        case '0': return &zero;
        case '1': return &one;
        case '2': return &two;
        case '3': return &three;
        case '4': return &four;
        case '5': return &five;
        case '6': return &six;
        case '7': return &seven;
        case '8': return &eight;
        case '9': return &nine;
        case 'A': return &a;
        case 'B': return &b;
        case 'C': return &c;
        case 'D': return &d;
        case 'E': return &e;
        case 'F': return &f;
        case 'G': return &g;
        case 'H': return &h;
        case 'I': return &i;
        case 'J': return &j;
        case 'K': return &k;
        case 'L': return &l;
        case 'M': return &m;
        case 'N': return &n;
        case 'O': return &o;
        case 'P': return &p;
        case 'Q': return &q;
        case 'R': return &r;
        case 'S': return &s;
        case 'T': return &t;
        case 'U': return &u;
        case 'V': return &v;
        case 'W': return &w;
        case 'X': return &x;
        case 'Y': return &y;
        case 'Z': return &z;
        default: return &space;
    }
}

void draw_screen_quad(float x, float y, float width, float height) {
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
}

// ============================================================================
// Text and UI rendering
// ============================================================================

void draw_text(float x, float y, float scale, const std::string& text) {
    FontAtlas& atlas = shared_ui_font_atlas();
    if (atlas.is_ready()) {
        auto& st = ae::gl_compat::state();
        atlas.draw_text(x, y, scale, text, st.current_r, st.current_g, st.current_b, st.current_a);
        return;
    }

    static bool warned_bitmap_fallback = false;
    if (!warned_bitmap_fallback) {
        warned_bitmap_fallback = true;
        ae::log_warning("draw_text: font atlas unavailable, using built-in 5x7 bitmap font. "
                        "Text will be low-resolution.");
    }

    glBegin(GL_QUADS);

    float cursor_x = x;
    for (const char character : text) {
        const auto* glyph = glyph_for_char(character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                const std::uint8_t bit = static_cast<std::uint8_t>(1U << (4 - column));
                if (((*glyph)[row] & bit) == 0) {
                    continue;
                }

                draw_screen_quad(
                    cursor_x + static_cast<float>(column) * scale,
                    y + static_cast<float>(row) * scale,
                    scale,
                    scale);
            }
        }

        cursor_x += 6.0F * scale;
    }

    glEnd();
}

void draw_ui_text(float x, float y, float scale, const std::string& text, UiTextStyle style) {
    switch (style) {
        case UiTextStyle::Header:
            glColor3f(0.98F, 0.99F, 1.0F);
            break;
        case UiTextStyle::Section:
            glColor3f(0.92F, 0.96F, 0.99F);
            break;
        case UiTextStyle::Body:
            glColor3f(0.80F, 0.84F, 0.88F);
            break;
        case UiTextStyle::Accent:
            glColor3f(0.96F, 0.84F, 0.16F);
            break;
        case UiTextStyle::Muted:
            glColor3f(0.62F, 0.68F, 0.76F);
            break;
        case UiTextStyle::Inverted:
            glColor3f(0.02F, 0.03F, 0.05F);
            break;
    }

    draw_text(x, y, scale, text);
}

// ============================================================================
// UI primitives
// ============================================================================

void draw_panel(float x, float y, float width, float height, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    draw_screen_quad(x, y, width, height);
    glEnd();
}

void draw_panel_outline(float x, float y, float width, float height, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glLineWidth(1.5F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
    glLineWidth(1.0F);
}

void draw_circle(float center_x, float center_y, float radius, float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(center_x, center_y);
    for (int step = 0; step <= 24; ++step) {
        const float angle = static_cast<float>(step) / 24.0F * 2.0F * kPi;
        glVertex2f(center_x + std::cos(angle) * radius, center_y + std::sin(angle) * radius);
    }
    glEnd();
}

void draw_button_chip(float x, float y, float width, float height, const char* label, float accent_r, float accent_g, float accent_b) {
    draw_panel(x, y, width, height, 0.09F, 0.12F, 0.16F, 0.9F);
    draw_panel_outline(x, y, width, height, accent_r, accent_g, accent_b, 0.8F);
    draw_ui_text(x + 8.0F, y + 5.0F, 1.8F, label, UiTextStyle::Header);
}

void draw_xbox_button_legend(unsigned int controller_buttons, float origin_x, float origin_y) {
    draw_panel(origin_x, origin_y, 232.0F, 88.0F, 0.04F, 0.06F, 0.09F, 0.9F);
    draw_panel_outline(origin_x, origin_y, 232.0F, 88.0F, 0.18F, 0.30F, 0.48F, 0.8F);
    draw_ui_text(origin_x + 10.0F, origin_y + 8.0F, 1.8F, "CONTROLLER", UiTextStyle::Section);

    const float cluster_x = origin_x + 168.0F;
    const float cluster_y = origin_y + 48.0F;
    draw_circle(cluster_x, cluster_y - 16.0F, 10.0F, 0.98F, 0.84F, 0.10F, 1.0F); // Y
    draw_circle(cluster_x + 16.0F, cluster_y, 10.0F, 0.84F, 0.18F, 0.18F, 1.0F); // B
    draw_circle(cluster_x - 16.0F, cluster_y, 10.0F, 0.15F, 0.40F, 0.95F, 1.0F); // X
    draw_circle(cluster_x, cluster_y + 16.0F, 10.0F, 0.18F, 0.78F, 0.22F, 1.0F); // A
    draw_ui_text(cluster_x - 3.0F, cluster_y - 22.0F, 1.4F, "Y", UiTextStyle::Inverted);
    draw_ui_text(cluster_x + 13.0F, cluster_y - 5.0F, 1.4F, "B", UiTextStyle::Inverted);
    draw_ui_text(cluster_x - 19.0F, cluster_y - 5.0F, 1.4F, "X", UiTextStyle::Inverted);
    draw_ui_text(cluster_x - 3.0F, cluster_y + 11.0F, 1.4F, "A", UiTextStyle::Inverted);

    draw_button_chip(origin_x + 10.0F, origin_y + 30.0F, 34.0F, 18.0F, "LB", 0.65F, 0.68F, 0.72F);
    draw_button_chip(origin_x + 48.0F, origin_y + 30.0F, 34.0F, 18.0F, "RB", 0.65F, 0.68F, 0.72F);
    draw_button_chip(origin_x + 10.0F, origin_y + 54.0F, 34.0F, 18.0F, "L3", 0.55F, 0.68F, 0.78F);
    draw_button_chip(origin_x + 48.0F, origin_y + 54.0F, 34.0F, 18.0F, "R3", 0.55F, 0.68F, 0.78F);
    draw_button_chip(origin_x + 88.0F, origin_y + 54.0F, 46.0F, 18.0F, "VIEW", 0.50F, 0.56F, 0.62F);
    draw_button_chip(origin_x + 88.0F, origin_y + 30.0F, 52.0F, 18.0F, "MENU", 0.50F, 0.56F, 0.62F);

    const struct ButtonHighlight {
        unsigned int bit;
        float x;
        float y;
        float width;
        float height;
        float r;
        float g;
        float b;
    } chips[] = {
        {1u << 4, origin_x + 10.0F, origin_y + 30.0F, 34.0F, 18.0F, 0.95F, 0.95F, 0.98F},   // LB
        {1u << 5, origin_x + 48.0F, origin_y + 30.0F, 34.0F, 18.0F, 0.95F, 0.95F, 0.98F},   // RB
        {1u << 9, origin_x + 10.0F, origin_y + 54.0F, 34.0F, 18.0F, 0.75F, 0.90F, 1.0F},    // L3
        {1u << 10, origin_x + 48.0F, origin_y + 54.0F, 34.0F, 18.0F, 0.75F, 0.90F, 1.0F},   // R3
        {1u << 6, origin_x + 88.0F, origin_y + 54.0F, 46.0F, 18.0F, 0.82F, 0.86F, 0.92F},   // Back/View
        {1u << 7, origin_x + 88.0F, origin_y + 30.0F, 52.0F, 18.0F, 0.82F, 0.86F, 0.92F},   // Start/Menu
    };

    for (const auto& chip : chips) {
        if ((controller_buttons & chip.bit) == 0) {
            continue;
        }

        draw_panel(chip.x + 2.0F, chip.y + 2.0F, chip.width - 4.0F, chip.height - 4.0F, chip.r, chip.g, chip.b, 0.18F);
    }

    const struct FaceHighlight {
        unsigned int bit;
        float x;
        float y;
        float r;
        float g;
        float b;
    } faces[] = {
        {1u << 0, cluster_x, cluster_y + 16.0F, 0.18F, 0.78F, 0.22F},  // A
        {1u << 1, cluster_x + 16.0F, cluster_y, 0.84F, 0.18F, 0.18F},  // B
        {1u << 2, cluster_x - 16.0F, cluster_y, 0.15F, 0.40F, 0.95F},  // X
        {1u << 3, cluster_x, cluster_y - 16.0F, 0.98F, 0.84F, 0.10F},  // Y
    };

    for (const auto& face : faces) {
        if ((controller_buttons & face.bit) == 0) {
            continue;
        }

        draw_circle(face.x, face.y, 13.0F, face.r, face.g, face.b, 0.28F);
    }
}

// ============================================================================
// Formatting helpers
// ============================================================================

std::string format_overlay_line(const char* label, double value, const char* suffix) {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.1f%s", label, value, suffix);
    return buffer;
}

std::string format_integer_overlay_line(const char* label, double value, const char* suffix) {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f%s", label, std::floor(value), suffix);
    return buffer;
}

std::string format_memory_line(const char* label, double used_mb, double total_mb) {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f/%.0fMB", label, used_mb, total_mb);
    return buffer;
}

// ============================================================================
// DebugRenderer::Impl — GPU state owned by the renderer
// ============================================================================

struct DebugRenderer::Impl {
    GLFWwindow* window {nullptr};  // cached for glfwGetTime / context management
    std::unique_ptr<RenderBackend> backend;
    bool auto_present {true};

    // Level-driven environment override (set via DebugRenderer::set_level_environment).
    bool has_level_env {false};
    float level_sky[3] {0.3F, 0.4F, 0.6F};
    float level_ambient[3] {0.05F, 0.05F, 0.1F};
    ShaderHandle shader_program;
    ShaderHandle depth_program;  // depth-only pre-pass shader
    int u_color_loc {-1};
    int u_fog_color_loc {-1};
    int u_fog_params_loc {-1};
    int u_camera_pos_loc {-1};
    int u_use_skinning_loc {-1};
    int u_joint_matrices_loc {-1};
    int u_modelview_loc {-1};
    int u_projection_loc {-1};
    GpuModel humanoid_vbo;      // LOD0: full detail
    GpuModel humanoid_vbo_lod1; // LOD1: medium
    GpuModel humanoid_vbo_lod2; // LOD2: low
    MapGeometry map_geometry;
    int cached_visible_cells[MapGeometry::kTotalCells]{};
    int cached_visible_count = 0;
    BufferHandle ground_grid_vbo;
    BufferHandle ground_grid_color_vbo;
    int ground_grid_vertex_count {0};

    // Dynamic VBOs for batched particle and decal rendering
    BufferHandle particle_vbo;
    BufferHandle particle_color_vbo;
    BufferHandle decal_vbo;
    BufferHandle decal_color_vbo;

    // Occlusion queries for dummy culling (GL_ARB_occlusion_query)
    static constexpr int kMaxOcclusionQueries = 16;
    GLuint occlusion_queries[kMaxOcclusionQueries] {};
    int occlusion_query_count {0};
    bool occlusion_results[kMaxOcclusionQueries] {}; // true = visible
    int occlusion_dummy_map[kMaxOcclusionQueries] {}; // query → dummy index

    // GPU timer queries (via backend)
    static constexpr int kNumGpuTimers = 4;
    QueryHandle gpu_timer_queries[kNumGpuTimers] {};
    bool gpu_timers_supported {false};
    double gpu_time_total_ms {0.0};
    double gpu_time_depth_ms {0.0};
    double gpu_time_map_ms {0.0};
    double gpu_time_entities_ms {0.0};
    double gpu_time_ui_ms {0.0};

    // Per-frame render stats
    RenderStats render_stats;

    // Camera matrices from the most recent render() (column-major). Exposed via
    // DebugRenderer getters so external passes (PBR level meshes) stay aligned.
    float last_view[16] {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float last_projection[16] {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float last_camera_pos[3] {0.0F, 0.0F, 0.0F};

    // Frame-time history ring buffer for sparkline in metrics overlay
    static constexpr int kSparklineHistorySize = 200;
    std::array<double, kSparklineHistorySize> frame_time_history {};
    int sparkline_index {0};
    int sparkline_count {0};

    // --- Render pass helpers (defined after render()) ---
    void draw_sky_pass(const DebugScene& scene, int width, int height,
                       float cr, float cg, float cb, float day_factor,
                       double cycle, double current_time);
    void draw_depth_pre_pass(const DebugScene& scene, const Frustum& frustum);
    void draw_main_color_pass(const DebugScene& scene, const Frustum& frustum,
                              const LocalMat4& view, float day_factor);
};

DebugRenderer::DebugRenderer() = default;

DebugRenderer::~DebugRenderer() {
    shutdown();
}

// No need for manual delete — unique_ptr handles it

namespace {
const float kIdentityMatrix4[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
const float kZeroVec3[3] = {0.0F, 0.0F, 0.0F};
}  // namespace

const float* DebugRenderer::view_matrix() const {
    return impl_ ? impl_->last_view : kIdentityMatrix4;
}

const float* DebugRenderer::projection_matrix() const {
    return impl_ ? impl_->last_projection : kIdentityMatrix4;
}

const float* DebugRenderer::camera_position() const {
    return impl_ ? impl_->last_camera_pos : kZeroVec3;
}

// ============================================================================
// Initialization
// ============================================================================

bool DebugRenderer::initialize(ae::PlatformWindow& window) {
    if (impl_ != nullptr) {
        return true;
    }

    auto* glfw_window = static_cast<GLFWwindow*>(window.native_handle());
    if (glfw_window == nullptr) {
        log_error("DebugRenderer received an invalid platform window handle.");
        return false;
    }

    // Create the backend and attach to the window
    auto backend = create_opengl_backend();
    if (!backend->initialize(glfw_window)) {
        log_error("DebugRenderer: failed to initialise render backend.");
        return false;
    }
    backend->set_swap_interval(1);

    // Core GL state (immediate-mode / fixed-function state)
    glfwMakeContextCurrent(glfw_window);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat ambient[] = {0.20F, 0.24F, 0.30F, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    GLfloat light_pos[] = {0.5F, 1.0F, 0.8F, 0.0F};
    GLfloat light_diffuse[] = {0.85F, 0.82F, 0.78F, 1.0F};
    GLfloat light_specular[] = {0.3F, 0.3F, 0.3F, 1.0F};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    glEnable(GL_LIGHT1);
    GLfloat moon_pos[] = {-0.5F, 0.6F, -0.8F, 0.0F};
    GLfloat moon_diffuse[] = {0.0F, 0.0F, 0.0F, 1.0F};
    GLfloat moon_specular[] = {0.0F, 0.0F, 0.0F, 1.0F};
    glLightfv(GL_LIGHT1, GL_POSITION, moon_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, moon_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, moon_specular);

    glClearColor(0.03F, 0.035F, 0.045F, 1.0F);

    impl_ = std::make_unique<Impl>();
    impl_->window = glfw_window;
    impl_->backend = std::move(backend);

    // --- Main shader program ---
    {
        const char* vs_src =
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPosition;\n"
            "layout(location = 1) in vec4 aJoints;\n"
            "layout(location = 2) in vec4 aWeights;\n"
            "layout(location = 3) in vec3 aNormal;\n"
            "uniform mat4 uModelView;\n"
            "uniform mat4 uProjection;\n"
            "uniform mat4 uJointMatrices[8];\n"
            "uniform int uUseSkinning;\n"
            "uniform vec3 uCameraPos;\n"
            "out vec3 vNormal;\n"
            "out vec3 vViewDir;\n"
            "out vec3 vWorldPos;\n"
            "out vec3 vViewPos;\n"
            "void main() {\n"
            "    vec4 position = vec4(aPosition, 1.0);\n"
            "    vec3 normal = aNormal;\n"
            "    if (uUseSkinning != 0) {\n"
            "        mat4 skinMat = uJointMatrices[int(aJoints.x)] * aWeights.x +\n"
            "                       uJointMatrices[int(aJoints.y)] * aWeights.y +\n"
            "                       uJointMatrices[int(aJoints.z)] * aWeights.z +\n"
            "                       uJointMatrices[int(aJoints.w)] * aWeights.w;\n"
            "        position = skinMat * vec4(aPosition, 1.0);\n"
            "        normal = mat3(skinMat) * aNormal;\n"
            "    }\n"
            "    vec4 worldPos = uModelView * position;\n"
            "    vWorldPos = worldPos.xyz;\n"
            "    vViewPos = vec3(worldPos);\n"
            "    gl_Position = uProjection * worldPos;\n"
            "    vNormal = normalize(mat3(uModelView) * normal);\n"
            "    vViewDir = normalize(-vec3(worldPos));\n"
            "}\n";

        const char* fs_src =
            "#version 330 core\n"
            "in vec3 vNormal;\n"
            "in vec3 vViewDir;\n"
            "in vec3 vWorldPos;\n"
            "in vec3 vViewPos;\n"
            "uniform vec4 uColor;\n"
            "uniform vec3 uFogColor;\n"
            "uniform vec2 uFogParams;\n"
            "uniform vec3 uCameraPos;\n"
            "uniform vec3 uLightModelAmbient;\n"
            "uniform vec3 uLight0Position;\n"
            "uniform vec3 uLight0Diffuse;\n"
            "uniform vec3 uLight0Specular;\n"
            "uniform vec3 uLight1Position;\n"
            "uniform vec3 uLight1Diffuse;\n"
            "uniform vec3 uLight1Specular;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    vec3 N = normalize(vNormal);\n"
            "    vec3 V = normalize(vViewDir);\n"
            "    vec3 ambient = uLightModelAmbient * uColor.rgb;\n"
            "    vec3 L0 = normalize(uLight0Position);\n"
            "    float diff0 = max(dot(N, L0), 0.0);\n"
            "    vec3 diffuse0 = uLight0Diffuse * uColor.rgb * diff0;\n"
            "    vec3 H0 = normalize(L0 + V);\n"
            "    float spec0 = pow(max(dot(N, H0), 0.0), 32.0);\n"
            "    vec3 specular0 = uLight0Specular * spec0 * 0.5;\n"
            "    vec3 L1 = normalize(uLight1Position);\n"
            "    float diff1 = max(dot(N, L1), 0.0);\n"
            "    vec3 diffuse1 = uLight1Diffuse * uColor.rgb * diff1;\n"
            "    vec3 H1 = normalize(L1 + V);\n"
            "    float spec1 = pow(max(dot(N, H1), 0.0), 32.0);\n"
            "    vec3 specular1 = uLight1Specular * spec1 * 0.3;\n"
            "    float noise = sin(vWorldPos.x * 12.0) * sin(vWorldPos.y * 12.0) * sin(vWorldPos.z * 12.0);\n"
            "    noise += sin(vWorldPos.x * 23.0 + vWorldPos.z * 17.0) * 0.5;\n"
            "    vec3 detailContrib = N * noise * 0.03;\n"
            "    vec3 finalColor = ambient + diffuse0 + specular0 + diffuse1 + specular1 + detailContrib;\n"
            "    float dist = length(vWorldPos);\n"
            "    float fogFactor = exp(-uFogParams.x * dist);\n"
            "    fogFactor = clamp(fogFactor, 0.0, 1.0);\n"
            "    finalColor = mix(uFogColor, finalColor, fogFactor);\n"
            "    fragColor = vec4(finalColor, uColor.a);\n"
            "}\n";

        const int main_attrib_locs[] = {0, 1, 2, 3};
        const char* main_attrib_names[] = {"aPosition", "aJoints", "aWeights", "aNormal"};
        ShaderProgramDesc main_desc{vs_src, fs_src, main_attrib_locs, main_attrib_names, 4};
        impl_->shader_program = impl_->backend->create_shader_program(main_desc);
    }

    if (impl_->shader_program) {
        impl_->u_color_loc       = impl_->backend->get_uniform_location(impl_->shader_program, "uColor");
        impl_->u_fog_color_loc   = impl_->backend->get_uniform_location(impl_->shader_program, "uFogColor");
        impl_->u_fog_params_loc  = impl_->backend->get_uniform_location(impl_->shader_program, "uFogParams");
        impl_->u_camera_pos_loc  = impl_->backend->get_uniform_location(impl_->shader_program, "uCameraPos");
        impl_->u_use_skinning_loc  = impl_->backend->get_uniform_location(impl_->shader_program, "uUseSkinning");
        impl_->u_joint_matrices_loc = impl_->backend->get_uniform_location(impl_->shader_program, "uJointMatrices");
        impl_->u_modelview_loc     = impl_->backend->get_uniform_location(impl_->shader_program, "uModelView");
        impl_->u_projection_loc    = impl_->backend->get_uniform_location(impl_->shader_program, "uProjection");
    }

    // --- Depth-only pre-pass shader ---
    {
        const char* depth_vs =
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPosition;\n"
            "layout(location = 1) in vec4 aJoints;\n"
            "layout(location = 2) in vec4 aWeights;\n"
            "uniform mat4 uModelView;\n"
            "uniform mat4 uProjection;\n"
            "uniform mat4 uJointMatrices[8];\n"
            "uniform int uUseSkinning;\n"
            "void main() {\n"
            "    vec4 position = vec4(aPosition, 1.0);\n"
            "    if (uUseSkinning != 0) {\n"
            "        mat4 skinMat = uJointMatrices[int(aJoints.x)] * aWeights.x +\n"
            "                       uJointMatrices[int(aJoints.y)] * aWeights.y +\n"
            "                       uJointMatrices[int(aJoints.z)] * aWeights.z +\n"
            "                       uJointMatrices[int(aJoints.w)] * aWeights.w;\n"
            "        position = skinMat * vec4(aPosition, 1.0);\n"
            "    }\n"
            "    gl_Position = uProjection * uModelView * position;\n"
            "}\n";
        const char* depth_fs =
            "#version 330 core\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    fragColor = vec4(1.0);\n"
            "}\n";

        const int depth_attrib_locs[] = {0, 1, 2};
        const char* depth_attrib_names[] = {"aPosition", "aJoints", "aWeights"};
        ShaderProgramDesc depth_desc{depth_vs, depth_fs, depth_attrib_locs, depth_attrib_names, 2};
        impl_->depth_program = impl_->backend->create_shader_program(depth_desc);
    }

    // Generate occlusion queries (raw GL – these are legacy and will be
    // replaced by GPU-driven culling in a future pass).
    glGenQueries(impl_->kMaxOcclusionQueries, impl_->occlusion_queries);

    // Check GPU timer support and create timer queries via backend
    impl_->gpu_timers_supported = impl_->backend->timer_queries_supported();
    if (impl_->gpu_timers_supported) {
        for (int i = 0; i < impl_->kNumGpuTimers; ++i) {
            impl_->gpu_timer_queries[i] = impl_->backend->create_query(true);
        }
    }

    // Build map geometry (spatially partitioned VBOs) — still uses raw GL
    impl_->map_geometry.build();

    // Build ground grid VBO via backend
    {
        std::vector<float> grid_positions;
        std::vector<float> grid_colors;
        constexpr int half_extent = 20;
        constexpr float spacing = 1.0F;
        for (int i = -half_extent; i <= half_extent; ++i) {
            float g = (i == 0 ? 0.32F : 0.18F);
            float offset = static_cast<float>(i) * spacing;
            float extent = static_cast<float>(half_extent) * spacing;
            grid_positions.push_back(-extent); grid_positions.push_back(0.0F); grid_positions.push_back(offset);
            grid_positions.push_back( extent); grid_positions.push_back(0.0F); grid_positions.push_back(offset);
            grid_positions.push_back(offset); grid_positions.push_back(0.0F); grid_positions.push_back(-extent);
            grid_positions.push_back(offset); grid_positions.push_back(0.0F); grid_positions.push_back( extent);
            for (int k = 0; k < 4; ++k) {
                grid_colors.push_back(g); grid_colors.push_back(g); grid_colors.push_back(g);
            }
        }
        impl_->ground_grid_vertex_count = static_cast<int>(grid_positions.size() / 3);
        impl_->ground_grid_vbo = impl_->backend->create_vertex_buffer(
            grid_positions.data(), grid_positions.size() * sizeof(float), false);
        impl_->ground_grid_color_vbo = impl_->backend->create_vertex_buffer(
            grid_colors.data(), grid_colors.size() * sizeof(float), false);
    }

    const GltfModel humanoid = generate_humanoid_mesh(HumanoidLod::High);
    impl_->humanoid_vbo = impl_->backend->create_gpu_model(humanoid);
    const GltfModel humanoid_lod1 = generate_humanoid_mesh(HumanoidLod::Medium);
    impl_->humanoid_vbo_lod1 = impl_->backend->create_gpu_model(humanoid_lod1);
    const GltfModel humanoid_lod2 = generate_humanoid_mesh(HumanoidLod::Low);
    impl_->humanoid_vbo_lod2 = impl_->backend->create_gpu_model(humanoid_lod2);

    ae::gl_compat::init();

    log_info("DebugRenderer initialized with OpenGL backend (VBO map, depth pre-pass, fog, specular).");
    return true;
}

// ============================================================================
// Shutdown
// ============================================================================

void DebugRenderer::shutdown() {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return;
    }

    // Ensure GL context is current before destroying resources.
    // The backend is still alive so context management is handled there.
    glfwMakeContextCurrent(impl_->window);

    impl_->backend->destroy_shader(impl_->shader_program);
    impl_->backend->destroy_shader(impl_->depth_program);
    impl_->shader_program = kInvalidShader;
    impl_->depth_program = kInvalidShader;

    impl_->map_geometry.destroy();

    impl_->backend->destroy_buffer(impl_->ground_grid_vbo);
    impl_->backend->destroy_buffer(impl_->ground_grid_color_vbo);
    impl_->ground_grid_vbo = kInvalidBuffer;
    impl_->ground_grid_color_vbo = kInvalidBuffer;

    impl_->backend->destroy_buffer(impl_->particle_vbo);
    impl_->backend->destroy_buffer(impl_->particle_color_vbo);
    impl_->backend->destroy_buffer(impl_->decal_vbo);
    impl_->backend->destroy_buffer(impl_->decal_color_vbo);
    impl_->particle_vbo = kInvalidBuffer;
    impl_->particle_color_vbo = kInvalidBuffer;
    impl_->decal_vbo = kInvalidBuffer;
    impl_->decal_color_vbo = kInvalidBuffer;

    if (impl_->occlusion_queries[0]) {
        glDeleteQueries(impl_->kMaxOcclusionQueries, impl_->occlusion_queries);
    }
    if (impl_->gpu_timers_supported) {
        for (int i = 0; i < impl_->kNumGpuTimers; ++i) {
            impl_->backend->destroy_query(impl_->gpu_timer_queries[i]);
            impl_->gpu_timer_queries[i] = kInvalidQuery;
        }
    }

    impl_->backend->destroy_gpu_model(impl_->humanoid_vbo);
    impl_->backend->destroy_gpu_model(impl_->humanoid_vbo_lod1);
    impl_->backend->destroy_gpu_model(impl_->humanoid_vbo_lod2);

    ae::gl_compat::shutdown();

    // Detach the backend (releases context)
    impl_->backend->shutdown();
    impl_->backend.reset();

    glfwMakeContextCurrent(nullptr);
    impl_.reset();
}

RenderBackend* DebugRenderer::backend() {
    return impl_ ? impl_->backend.get() : nullptr;
}

// ============================================================================
// Impl render pass helpers
// ============================================================================

void DebugRenderer::Impl::draw_sky_pass(const DebugScene& scene, int width, int height,
                   float cr, float cg, float cb, float day_factor, double cycle, double current_time) {
    float zenith_r = cr * 0.35F;
    float zenith_g = cg * 0.35F;
    float zenith_b = cb * 0.55F;
    const float grad_top = 0.0F;
    const float grad_bot = static_cast<float>(height) * 0.40F;
    glBegin(GL_QUADS);
    glColor4f(zenith_r, zenith_g, zenith_b, 1.0F);
    glVertex2f(0.0F, grad_top);
    glVertex2f(static_cast<float>(width), grad_top);
    glColor4f(cr, cg, cb, 1.0F);
    glVertex2f(static_cast<float>(width), grad_bot);
    glVertex2f(0.0F, grad_bot);
    glEnd();

    // --- Procedural sky: sun/moon disc and starfield ---
    float sun_angle = static_cast<float>(cycle * 2.0 * 3.1415926535);
    float sun_x = std::sin(sun_angle) * 0.6F;  // screen-relative
    float sun_y = 0.25F - std::cos(sun_angle) * 0.35F;
    float sun_screen_x = sun_x * static_cast<float>(width) + static_cast<float>(width) * 0.5F;
    float sun_screen_y = sun_y * static_cast<float>(height);

    // Sun disc
    if (static_cast<float>(day_factor) > 0.05F) {
        float sun_alpha = static_cast<float>(day_factor);
        float sun_r = 1.0F, sun_g = 0.95F, sun_b = 0.7F;
        int sun_segments = 32;
        float sun_radius = 28.0F;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        // Outer glow
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sun_r * 0.3F, sun_g * 0.2F, sun_b * 0.1F, sun_alpha * 0.4F);
        glVertex2f(sun_screen_x, sun_screen_y);
        for (int i = 0; i <= sun_segments; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sun_segments) * 2.0F * 3.14159265F;
            glVertex2f(sun_screen_x + std::cos(a) * sun_radius * 2.5F, sun_screen_y + std::sin(a) * sun_radius * 2.5F);
        }
        glEnd();
        // Inner disc
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(sun_r, sun_g, sun_b, sun_alpha);
        glVertex2f(sun_screen_x, sun_screen_y);
        for (int i = 0; i <= sun_segments; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sun_segments) * 2.0F * 3.14159265F;
            glVertex2f(sun_screen_x + std::cos(a) * sun_radius, sun_screen_y + std::sin(a) * sun_radius);
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    // Moon disc (opposite angle to sun)
    float moon_alpha = 1.0F - static_cast<float>(day_factor);
    if (moon_alpha > 0.05F) {
        float moon_sx = -sun_screen_x + static_cast<float>(width);
        float moon_sy = sun_screen_y * 0.7F + static_cast<float>(height) * 0.15F;
        float moon_radius = 20.0F;
        int moon_seg = 24;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        // Glow
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.5F, 0.55F, 0.7F, moon_alpha * 0.3F);
        glVertex2f(moon_sx, moon_sy);
        for (int i = 0; i <= moon_seg; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(moon_seg) * 2.0F * 3.14159265F;
            glVertex2f(moon_sx + std::cos(a) * moon_radius * 2.0F, moon_sy + std::sin(a) * moon_radius * 2.0F);
        }
        glEnd();
        // Disc
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.8F, 0.85F, 0.95F, moon_alpha);
        glVertex2f(moon_sx, moon_sy);
        for (int i = 0; i <= moon_seg; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(moon_seg) * 2.0F * 3.14159265F;
            glVertex2f(moon_sx + std::cos(a) * moon_radius, moon_sy + std::sin(a) * moon_radius);
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    // Starfield (visible at night)
    if (moon_alpha > 0.1F) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glPointSize(1.5F);
        glBegin(GL_POINTS);
        // Deterministic pseudo-random star positions using prime multipliers
        for (int i = 0; i < 200; ++i) {
            float sx = std::fmod(static_cast<float>(i * 7919 + 104729), static_cast<float>(width));
            float sy = std::fmod(static_cast<float>(i * 6271 + 224737), static_cast<float>(height) * 0.55F);
            float brightness = std::fmod(static_cast<float>(i * 373), 1.0F) * 0.5F + 0.3F;
            float twinkle = std::sin(static_cast<float>(current_time * 2.0 + i * 0.7)) * 0.3F + 0.7F;
            float alpha = moon_alpha * brightness * twinkle;
            glColor4f(0.9F, 0.95F, 1.0F, alpha);
            glVertex2f(sx, sy);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
}

void DebugRenderer::Impl::draw_depth_pre_pass(const DebugScene& scene, const Frustum& frustum) {
    if (!depth_program) return;

    if (gpu_timers_supported) {
        backend->begin_query(gpu_timer_queries[1]);
    }
    backend->set_color_write(false, false, false, false);
    backend->set_depth_func_less();
    backend->use_shader(depth_program);

    // Draw map cells
    {
        const int vc = cached_visible_count;
        if (vc > 0) {
            for (int vi = 0; vi < vc; ++vi) {
                const auto& cell = map_geometry.cells[static_cast<std::size_t>(cached_visible_cells[vi])];
                if (cell.triangle_count > 0) {
                    glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_positions);
                    glVertexPointer(3, GL_FLOAT, 0, nullptr);
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glDrawArrays(GL_TRIANGLES, 0, cell.triangle_count);
                    glDisableClientState(GL_VERTEX_ARRAY);
                }
            }
        }
    }

    // Draw custom level boxes
    for (int i = 0; i < scene.level_box_count && i < 64; ++i) {
        const DebugBox& box = scene.level_boxes[i];
        glBegin(GL_QUADS);
        glVertex3f(box.min.x, box.min.y, box.max.z); glVertex3f(box.max.x, box.min.y, box.max.z);
        glVertex3f(box.max.x, box.max.y, box.max.z); glVertex3f(box.min.x, box.max.y, box.max.z);
        glEnd();
    }

    // Draw player and dummies (only if visible)
    if (scene.show_player_marker) {
        glPushMatrix();
        glTranslatef(scene.player_position.x, scene.player_position.y, scene.player_position.z);
        glRotatef(scene.player_yaw * 180.0F / kPi, 0.0F, 1.0F, 0.0F);
        glScalef(scene.player_height, scene.player_height, scene.player_height);
        draw_gpu_model(*backend, humanoid_vbo, depth_program,
                      -1, u_use_skinning_loc, u_joint_matrices_loc,
                      1.0F, 1.0F, 1.0F, 1.0F, true, nullptr, 0);
        glPopMatrix();
    }
    for (int i = 0; i < scene.dummy_count && i < 16; ++i) {
        if (!scene.dummy_alive[i]) continue;
        const auto& dpos = scene.dummy_positions[i];
        float dummy_radius = scene.player_height * 0.6F;
        if (!frustum.intersects_sphere(dpos.x, dpos.y + scene.player_height * 0.5F, dpos.z, dummy_radius)) continue;
        glPushMatrix();
        glTranslatef(dpos.x, dpos.y, dpos.z);
        glRotatef(scene.dummy_yaws[i] * 180.0F / kPi, 0.0F, 1.0F, 0.0F);
        glScalef(scene.player_height, scene.player_height, scene.player_height);
        draw_gpu_model(*backend, humanoid_vbo, depth_program,
                      -1, u_use_skinning_loc, u_joint_matrices_loc,
                      1.0F, 1.0F, 1.0F, 1.0F, false, nullptr, 0);
        glPopMatrix();
    }

    backend->use_shader(kInvalidShader);
    backend->set_color_write(true, true, true, true);
    backend->set_depth_func_equal();  // main pass: only draw pixels at the depth written in pre-pass
    if (gpu_timers_supported) {
        backend->end_query(gpu_timer_queries[1]);
    }
}

void DebugRenderer::Impl::draw_main_color_pass(const DebugScene& scene,
                          const Frustum& frustum, const LocalMat4& view, float day_factor) {
    // Ground grid + axes
    glDisable(GL_LIGHTING);
    if (ground_grid_vbo && ground_grid_vertex_count > 0) {
        float b = 0.20F + 0.80F * static_cast<float>(day_factor);
        glColor3f(b, b, b);
        glLineWidth(1.0F);
        backend->draw_arrays_positions(ground_grid_vbo, 0, ground_grid_vertex_count);
    }
    draw_axes();
    glEnable(GL_LIGHTING);

    // Update lighting for time of day
    float gamma = scene.gamma > 0.0F ? scene.gamma : 1.0F;
    float light_brightness = (0.30F + 0.70F * static_cast<float>(day_factor)) * gamma;
    constexpr float kMinAmbientR = 0.06F, kMinAmbientG = 0.08F, kMinAmbientB = 0.12F;
    float ambient_r = (kMinAmbientR + (0.18F - kMinAmbientR) * static_cast<float>(day_factor)) * gamma;
    float ambient_g = (kMinAmbientG + (0.22F - kMinAmbientG) * static_cast<float>(day_factor)) * gamma;
    float ambient_b = (kMinAmbientB + (0.28F - kMinAmbientB) * static_cast<float>(day_factor)) * gamma;
    if (has_level_env) {
        ambient_r = level_ambient[0] * gamma;
        ambient_g = level_ambient[1] * gamma;
        ambient_b = level_ambient[2] * gamma;
    }
    GLfloat ambient[] = {ambient_r, ambient_g, ambient_b, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    GLfloat diffuse[] = {0.95F * light_brightness, 0.9F * light_brightness, 0.8F * light_brightness, 1.0F};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    GLfloat specular[] = {0.5F * light_brightness, 0.45F * light_brightness, 0.35F * light_brightness, 1.0F};
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    float moonlight = 1.0F - static_cast<float>(day_factor);
    float moon_strength = 0.25F * moonlight;
    GLfloat moon_diffuse[] = {0.08F * moon_strength, 0.14F * moon_strength, 0.28F * moon_strength, 1.0F};
    glLightfv(GL_LIGHT1, GL_DIFFUSE, moon_diffuse);
    GLfloat moon_specular[] = {0.04F * moon_strength, 0.07F * moon_strength, 0.14F * moon_strength, 1.0F};
    glLightfv(GL_LIGHT1, GL_SPECULAR, moon_specular);

    // Draw spatially-culled map
    if (scene.draw_default_map) {
        render_stats.map_cells_visible = cached_visible_count;
        render_stats.map_cells_total = MapGeometry::kTotalCells;

        for (int vi = 0; vi < cached_visible_count; ++vi) {
            const auto& cell = map_geometry.cells[static_cast<std::size_t>(cached_visible_cells[vi])];
            if (cell.triangle_count > 0) {
                glEnableClientState(GL_VERTEX_ARRAY);
                glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_positions);
                glVertexPointer(3, GL_FLOAT, 0, nullptr);

                glEnableClientState(GL_NORMAL_ARRAY);
                glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_normals);
                glNormalPointer(GL_FLOAT, 0, nullptr);

                glEnableClientState(GL_COLOR_ARRAY);
                glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_colors);
                glColorPointer(3, GL_FLOAT, 0, nullptr);

                glDrawArrays(GL_TRIANGLES, 0, cell.triangle_count);

                glDisableClientState(GL_VERTEX_ARRAY);
                glDisableClientState(GL_NORMAL_ARRAY);
                glDisableClientState(GL_COLOR_ARRAY);
            }
        }

        // Draw line geometry for map (direction markers etc)
        glDisable(GL_LIGHTING);
        for (int vi = 0; vi < cached_visible_count; ++vi) {
            const auto& cell = map_geometry.cells[static_cast<std::size_t>(cached_visible_cells[vi])];
            if (cell.line_count > 0) {
                glEnableClientState(GL_VERTEX_ARRAY);
                glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_positions);
                glVertexPointer(3, GL_FLOAT, 0,
                    reinterpret_cast<void*>(static_cast<std::size_t>(cell.triangle_count) * 3 * sizeof(float)));

                glEnableClientState(GL_COLOR_ARRAY);
                glBindBuffer(GL_ARRAY_BUFFER, cell.vbo_colors);
                glColorPointer(3, GL_FLOAT, 0,
                    reinterpret_cast<void*>(static_cast<std::size_t>(cell.triangle_count) * 3 * sizeof(float)));

                glLineWidth(2.0F);
                glDrawArrays(GL_LINES, 0, cell.line_count);
                glLineWidth(1.0F);

                glDisableClientState(GL_VERTEX_ARRAY);
                glDisableClientState(GL_COLOR_ARRAY);
            }
        }
        glEnable(GL_LIGHTING);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Draw custom level boxes
    for (int i = 0; i < scene.level_box_count && i < 64; ++i) {
        const DebugBox& box = scene.level_boxes[i];
        set_color(box.red, box.green, box.blue);
        draw_box(box.min, box.max);
    }

    // Draw player marker
    if (scene.show_player_marker) {
        glPushMatrix();
        glTranslatef(scene.player_position.x, scene.player_position.y, scene.player_position.z);
        glRotatef(scene.player_yaw * 180.0F / kPi, 0.0F, 1.0F, 0.0F);
        glScalef(scene.player_height, scene.player_height, scene.player_height);

        draw_gpu_model(*backend, humanoid_vbo, shader_program,
                      u_color_loc, u_use_skinning_loc, u_joint_matrices_loc,
                      1.0F, 1.0F, 1.0F, 1.0F, true, nullptr, 0);

        glPopMatrix();
    }

    // Draw target dummies with frustum culling and LOD selection
    {
        render_stats.total_dummies = scene.dummy_count;
        for (int i = 0; i < scene.dummy_count && i < 16; ++i) {
            if (!scene.dummy_alive[i]) {
                render_stats.culled_dummies++;
                continue;
            }

            const auto& dpos = scene.dummy_positions[i];

            float dummy_radius = scene.player_height * 0.6F;
            if (!frustum.intersects_sphere(dpos.x, dpos.y + scene.player_height * 0.5F, dpos.z, dummy_radius)) {
                render_stats.culled_dummies++;
                continue;
            }

            float dx = dpos.x - scene.camera_position.x;
            float dy = dpos.y - scene.camera_position.y;
            float dz = dpos.z - scene.camera_position.z;
            float dist_sq = dx * dx + dy * dy + dz * dz;
            LodLevel lod = select_lod(dist_sq);

            const GpuModel* lod_vbo = &humanoid_vbo;
            if (lod == LodLevel::Medium) {
                lod_vbo = &humanoid_vbo_lod1;
                render_stats.lod1_count++;
            } else if (lod == LodLevel::Low) {
                lod_vbo = &humanoid_vbo_lod2;
                render_stats.lod2_count++;
            } else {
                render_stats.lod0_count++;
            }

            glPushMatrix();
            glTranslatef(dpos.x, dpos.y, dpos.z);
            glRotatef(scene.dummy_yaws[i] * 180.0F / kPi, 0.0F, 1.0F, 0.0F);
            glScalef(scene.player_height, scene.player_height, scene.player_height);

            float dr = 0.8F, dg = 0.15F, db = 0.15F;
            if (scene.dummy_recently_hit[i]) {
                dr = 1.0F; dg = 0.4F; db = 0.4F;
            }
            draw_gpu_model(*backend, *lod_vbo, shader_program,
                          u_color_loc, u_use_skinning_loc, u_joint_matrices_loc,
                          dr, dg, db, 1.0F, false, nullptr, 0);

            glPopMatrix();
            render_stats.drawn_dummies++;
        }
    }

    // Draw Muzzle Flash
    if (scene.muzzle_flash_time > 0.0F) {
        glDisable(GL_LIGHTING);
        float fx = scene.camera_target.x - scene.camera_position.x;
        float fy = scene.camera_target.y - scene.camera_position.y;
        float fz = scene.camera_target.z - scene.camera_position.z;
        float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
        if (flen > 0.001F) {
            fx /= flen; fy /= flen; fz /= flen;
            float rx = fz;
            float rz = -fx;
            float rlen = std::sqrt(rx*rx + rz*rz);
            if (rlen > 0.001F) {
                rx /= rlen; rz /= rlen;
            }
            float mx = scene.camera_position.x + fx * 0.6F + rx * 0.15F;
            float my = scene.camera_position.y + fy * 0.6F - 0.15F;
            float mz = scene.camera_position.z + fz * 0.6F + rz * 0.15F;

            set_color(1.0F, 0.8F, 0.1F);
            float r = 0.06F * scene.muzzle_flash_time;
            glBegin(GL_TRIANGLES);
            glVertex3f(mx, my + r, mz); glVertex3f(mx, my, mz + r); glVertex3f(mx + r, my, mz);
            glVertex3f(mx, my + r, mz); glVertex3f(mx + r, my, mz); glVertex3f(mx, my, mz - r);
            glVertex3f(mx, my + r, mz); glVertex3f(mx, my, mz - r); glVertex3f(mx - r, my, mz);
            glVertex3f(mx, my + r, mz); glVertex3f(mx - r, my, mz); glVertex3f(mx, my, mz + r);
            glEnd();
        }
        glEnable(GL_LIGHTING);
    }

    // Draw projectiles with frustum culling
    glDisable(GL_LIGHTING);
    render_stats.total_projectiles = scene.projectile_count;
    for (int i = 0; i < scene.projectile_count && i < 64; ++i) {
        const auto& pp = scene.projectile_positions[i];

        if (!frustum.intersects_sphere(pp.x, pp.y, pp.z, 0.12F)) {
            continue;
        }
        render_stats.drawn_projectiles++;

        set_color(1.0F, 0.55F, 0.1F);
        const float r = 0.08F;
        glBegin(GL_TRIANGLES);
        glVertex3f(pp.x, pp.y + r, pp.z); glVertex3f(pp.x, pp.y, pp.z + r); glVertex3f(pp.x + r, pp.y, pp.z);
        glVertex3f(pp.x, pp.y + r, pp.z); glVertex3f(pp.x + r, pp.y, pp.z); glVertex3f(pp.x, pp.y, pp.z - r);
        glVertex3f(pp.x, pp.y + r, pp.z); glVertex3f(pp.x, pp.y, pp.z - r); glVertex3f(pp.x - r, pp.y, pp.z);
        glVertex3f(pp.x, pp.y + r, pp.z); glVertex3f(pp.x - r, pp.y, pp.z); glVertex3f(pp.x, pp.y, pp.z + r);
        glVertex3f(pp.x, pp.y - r, pp.z); glVertex3f(pp.x + r, pp.y, pp.z); glVertex3f(pp.x, pp.y, pp.z + r);
        glVertex3f(pp.x, pp.y - r, pp.z); glVertex3f(pp.x, pp.y, pp.z - r); glVertex3f(pp.x + r, pp.y, pp.z);
        glVertex3f(pp.x, pp.y - r, pp.z); glVertex3f(pp.x - r, pp.y, pp.z); glVertex3f(pp.x, pp.y, pp.z - r);
        glVertex3f(pp.x, pp.y - r, pp.z); glVertex3f(pp.x, pp.y, pp.z + r); glVertex3f(pp.x - r, pp.y, pp.z);
        glEnd();
    }
    glEnable(GL_LIGHTING);

    // Draw particles as camera-facing billboards
    if (scene.particle_count > 0) {
        draw_particles(*backend, particle_vbo, particle_color_vbo, render_stats, scene, view, frustum);
        render_stats.total_particle_count = scene.particle_count;
    }

    // Draw bullet hole decals with depth offset
    if (scene.decal_count > 0) {
        draw_decals(*backend, decal_vbo, decal_color_vbo, render_stats, scene, frustum);
        render_stats.total_decal_count = scene.decal_count;
    }
}
// ============================================================================
// Main render entry point — orchestrates all render passes
// ============================================================================

void DebugRenderer::render(DebugScene& scene, const std::function<void()>& draw_world_extra) {
    if (impl_ == nullptr || impl_->window == nullptr) {
        return;
    }

    // Day/night cycle — time tracking
    static double start_time = -1.0;
    if (start_time < 0.0) start_time = glfwGetTime();
    const double current_time = glfwGetTime() - start_time;

    double cycle = std::fmod(current_time / 60.0, 1.0);
    double day_factor = std::sin(cycle * 3.1415926535 * 2.0) * 0.5 + 0.5;
    if (scene.always_day) day_factor = 1.0;

    // Interpolate clear color between night and day.
    float night_r = 0.08F, night_g = 0.10F, night_b = 0.18F;
    float day_r = 0.35F, day_g = 0.55F, day_b = 0.85F;
    float cr = night_r + (day_r - night_r) * static_cast<float>(day_factor);
    float cg = night_g + (day_g - night_g) * static_cast<float>(day_factor);
    float cb = night_b + (day_b - night_b) * static_cast<float>(day_factor);

    // Level-driven sky: when a level provides environment settings, its sky
    // color drives the clear color, the sky-gradient pass, and the fog color
    // (all derived from cr/cg/cb below).
    if (impl_->has_level_env) {
        cr = impl_->level_sky[0];
        cg = impl_->level_sky[1];
        cb = impl_->level_sky[2];
    }
    glClearColor(cr, cg, cb, 1.0F);

    int framebuffer_width = 1;
    int framebuffer_height = 1;
    impl_->backend->get_framebuffer_size(framebuffer_width, framebuffer_height);
    const int width = framebuffer_width > 0 ? framebuffer_width : 1;
    const int height = framebuffer_height > 0 ? framebuffer_height : 1;
    const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);

    impl_->backend->set_viewport(0, 0, width, height);
    impl_->backend->clear_color_and_depth();

    ae::gl_compat::begin_frame(width, height);

    // Record frame time for sparkline
    impl_->frame_time_history[impl_->sparkline_index] = scene.frame_time_ms;
    impl_->sparkline_index = (impl_->sparkline_index + 1) % impl_->kSparklineHistorySize;
    if (impl_->sparkline_count < impl_->kSparklineHistorySize) {
        impl_->sparkline_count++;
    }

    // --- GPU timer: total frame ---
    if (impl_->gpu_timers_supported) {
        impl_->backend->begin_query(impl_->gpu_timer_queries[0]);
    }

    // --- Sky gradient: darkens toward the zenith for depth cues ---
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);

        impl_->draw_sky_pass(scene, width, height, cr, cg, cb, static_cast<float>(day_factor), cycle, current_time);

        glEnable(GL_DEPTH_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    const LocalMat4 projection = perspective(60.0F * kPi / 180.0F, aspect_ratio, 0.05F, 250.0F);
    const LocalMat4 view = look_at(scene.camera_position, scene.camera_target, {0.0F, 1.0F, 0.0F});

    for (int i = 0; i < 16; ++i) {
        impl_->last_view[i] = view.values[static_cast<std::size_t>(i)];
        impl_->last_projection[i] = projection.values[static_cast<std::size_t>(i)];
    }
    impl_->last_camera_pos[0] = scene.camera_position.x;
    impl_->last_camera_pos[1] = scene.camera_position.y;
    impl_->last_camera_pos[2] = scene.camera_position.z;

    // --- Build MVP and extract frustum for culling ---
    LocalMat4 mvp;
    {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0F;
                for (int k = 0; k < 4; ++k) {
                    sum += projection.values[static_cast<std::size_t>(k * 4 + row)] *
                           view.values[static_cast<std::size_t>(col * 4 + k)];
                }
                mvp.values[static_cast<std::size_t>(col * 4 + row)] = sum;
            }
        }
    }
    const Frustum frustum = Frustum::from_matrix(mvp.values.data());
    impl_->render_stats.reset();

    // Compute visible cells once for both passes
    impl_->cached_visible_count = 0;
    impl_->map_geometry.collect_visible(frustum, impl_->cached_visible_cells, impl_->cached_visible_count);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.values.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.values.data());

    // --- Set fog uniforms for main shader (backend) ---
    if (impl_->shader_program) {
        impl_->backend->use_shader(impl_->shader_program);
        float fog_density = 0.0008F + (1.0F - static_cast<float>(day_factor)) * 0.0015F;
        impl_->backend->set_uniform_vec3(impl_->u_fog_color_loc, cr, cg, cb);
        impl_->backend->set_uniform_vec2(impl_->u_fog_params_loc, fog_density, 0.0F);
        impl_->backend->set_uniform_vec3(impl_->u_camera_pos_loc, scene.camera_position.x, scene.camera_position.y, scene.camera_position.z);
        impl_->backend->use_shader(kInvalidShader);
    }

    // ============================================================
    // DEPTH PRE-PASS
    // ============================================================
    impl_->draw_depth_pre_pass(scene, frustum);

    // ============================================================
    // MAIN COLOR PASS
    // ============================================================
    if (impl_->gpu_timers_supported) {
        impl_->backend->begin_query(impl_->gpu_timer_queries[2]);
    }

    impl_->draw_main_color_pass(scene, frustum, view, static_cast<float>(day_factor));

    // Extra world-space geometry (e.g. PBR level meshes) drawn in the 3D phase,
    // after the main color pass but before screen-space overlays, so it cannot
    // overwrite the HUD/crosshair/menu.
    if (draw_world_extra) {
        draw_world_extra();
    }

    // --- End main color pass timer, begin UI pass timer ---
    if (impl_->gpu_timers_supported) {
        impl_->backend->end_query(impl_->gpu_timer_queries[2]);
        impl_->backend->begin_query(impl_->gpu_timer_queries[3]);
    }

    // ============================================================
    // UI / OVERLAY PASS (screen-space, no depth test)
    // ============================================================
    if (scene.show_crosshair) {
        draw_crosshair_overlay(scene, width, height);
    }
    if (scene.metrics_visible) {
        draw_metrics_overlay(scene, width, height, impl_->frame_time_history, impl_->sparkline_count);
    }
    if (scene.gpu_profiler_visible) {
        draw_gpu_profiler_overlay(scene, width, height);
    }

    if (scene.hud_visible) {
        draw_hud(scene, width, height, static_cast<float>(day_factor));
    }

    // Draw Projected Floating Damage Numbers in 2D space
    {
        struct ProjectedText {
            float x;
            float y;
            float val;
            bool is_crit;
            float alpha;
        };
        std::vector<ProjectedText> proj_texts;
        {
            GLfloat mv[16];
            GLfloat proj[16];
            glGetFloatv(GL_MODELVIEW_MATRIX, mv);
            glGetFloatv(GL_PROJECTION_MATRIX, proj);
            for (int i = 0; i < scene.hit_number_count && i < 16; ++i) {
                const auto& hp = scene.hit_number_positions[i];
                float vx = mv[0] * hp.x + mv[4] * hp.y + mv[8] * hp.z + mv[12];
                float vy = mv[1] * hp.x + mv[5] * hp.y + mv[9] * hp.z + mv[13];
                float vz = mv[2] * hp.x + mv[6] * hp.y + mv[10] * hp.z + mv[14];
                float vw = mv[3] * hp.x + mv[7] * hp.y + mv[11] * hp.z + mv[15];

                float cx = proj[0] * vx + proj[4] * vy + proj[8] * vz + proj[12] * vw;
                float cy = proj[1] * vx + proj[5] * vy + proj[9] * vz + proj[13] * vw;
                float cz = proj[2] * vx + proj[6] * vy + proj[10] * vz + proj[14] * vw;
                float cw = proj[3] * vx + proj[7] * vy + proj[11] * vz + proj[15] * vw;

                if (cw > 0.001F) {
                    float ndcx = cx / cw;
                    float ndcy = cy / cw;
                    float ndcz = cz / cw;
                    if (ndcz >= -1.0F && ndcz <= 1.0F) {
                        float sx = (ndcx + 1.0F) * 0.5F * static_cast<float>(width);
                        float sy = (1.0F - ndcy) * 0.5F * static_cast<float>(height);
                        proj_texts.push_back({sx, sy, scene.hit_number_values[i], scene.hit_number_is_critical[i], scene.hit_number_lifetimes[i]});
                    }
                }
            }
        }

        if (!proj_texts.empty()) {
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_LIGHTING);

            for (const auto& pt : proj_texts) {
                if (pt.is_crit) {
                    glColor4f(1.0F, 0.85F, 0.1F, pt.alpha);
                } else {
                    glColor4f(1.0F, 1.0F, 1.0F, pt.alpha);
                }
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.0f", pt.val);
                float size = pt.is_crit ? 2.2F : 1.6F;
                draw_text(pt.x - 10.0F, pt.y - 10.0F, size, buf);
            }

            glEnable(GL_DEPTH_TEST);
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
        }
    }

    if (scene.menu_visible) {
        draw_menu_overlay(scene, width, height);
    }
    draw_scene_overlay(scene, width, height);

    // End UI pass timer
    if (impl_->gpu_timers_supported) {
        impl_->backend->end_query(impl_->gpu_timer_queries[3]);
    }

    // ============================================================
    // OCCLUSION QUERIES: issue queries for next frame's dummy culling
    // ============================================================
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    impl_->occlusion_query_count = 0;
    for (int i = 0; i < scene.dummy_count && i < 16 && impl_->occlusion_query_count < impl_->kMaxOcclusionQueries; ++i) {
        if (!scene.dummy_alive[i]) continue;
        const auto& dpos = scene.dummy_positions[i];
        float dummy_radius = scene.player_height * 0.6F;
        if (!frustum.intersects_sphere(dpos.x, dpos.y + scene.player_height * 0.5F, dpos.z, dummy_radius)) continue;

        int qi = impl_->occlusion_query_count++;
        impl_->occlusion_dummy_map[qi] = i;
        glBeginQuery(GL_SAMPLES_PASSED, impl_->occlusion_queries[qi]);
        glPushMatrix();
        glTranslatef(dpos.x, dpos.y + scene.player_height * 0.5F, dpos.z);
        glScalef(dummy_radius, dummy_radius, dummy_radius);
        glBegin(GL_QUADS);
        glVertex3f(-1, -1, 0); glVertex3f(1, -1, 0); glVertex3f(1, 1, 0); glVertex3f(-1, 1, 0);
        glEnd();
        glPopMatrix();
        glEndQuery(GL_SAMPLES_PASSED);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    // Read occlusion query results from previous frame
    for (int i = 0; i < impl_->occlusion_query_count; ++i) {
        GLint available = 0;
        glGetQueryObjectiv(impl_->occlusion_queries[i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLint samples = 0;
            glGetQueryObjectiv(impl_->occlusion_queries[i], GL_QUERY_RESULT, &samples);
            impl_->occlusion_results[i] = (samples > 0);
        }
    }

    // ============================================================
    // READ GPU TIMERS
    // ============================================================
    if (impl_->gpu_timers_supported) {
        impl_->backend->end_query(impl_->gpu_timer_queries[0]);  // end total frame timer

        auto read_timer = [&](QueryHandle query, double& out_ms) {
            if (impl_->backend->is_query_result_available(query)) {
                out_ms = static_cast<double>(impl_->backend->get_query_result_uint64(query)) / 1000000.0;
            }
        };
        read_timer(impl_->gpu_timer_queries[0], impl_->gpu_time_total_ms);
        read_timer(impl_->gpu_timer_queries[1], impl_->gpu_time_depth_ms);
        read_timer(impl_->gpu_timer_queries[2], impl_->gpu_time_map_ms);
        read_timer(impl_->gpu_timer_queries[3], impl_->gpu_time_ui_ms);
    }

    // Copy per-frame stats back to the scene
    scene.render_stats = impl_->render_stats;
    scene.gpu_time_total_ms = impl_->gpu_time_total_ms;
    scene.gpu_time_depth_ms = impl_->gpu_time_depth_ms;
    scene.gpu_time_map_ms = impl_->gpu_time_map_ms;
    scene.gpu_time_entities_ms = 0.0; // removed — placeholder
    scene.gpu_time_ui_ms = impl_->gpu_time_ui_ms;
    scene.gpu_usage_available = impl_->gpu_timers_supported;

    // Legacy auto-present: present() is only called here when auto_present is
    // true (the default, for simple/non-staged callers). Staged frame loops
    // disable it via set_auto_present(false) and call present() in their own
    // present stage, so render() means "draw" and present() means "swap".
    if (impl_->auto_present) {
        present();
    }
}

void DebugRenderer::present() {
    if (impl_ == nullptr || impl_->backend == nullptr) {
        return;
    }

    impl_->backend->end_frame();
}

void DebugRenderer::set_auto_present(bool enabled) {
    if (impl_ == nullptr) {
        return;
    }

    impl_->auto_present = enabled;
}

void DebugRenderer::set_level_environment(float sky_r, float sky_g, float sky_b,
                                          float ambient_r, float ambient_g, float ambient_b) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->level_sky[0] = sky_r;
    impl_->level_sky[1] = sky_g;
    impl_->level_sky[2] = sky_b;
    impl_->level_ambient[0] = ambient_r;
    impl_->level_ambient[1] = ambient_g;
    impl_->level_ambient[2] = ambient_b;
    impl_->has_level_env = true;
}

void DebugRenderer::clear_level_environment() {
    if (impl_ != nullptr) {
        impl_->has_level_env = false;
    }
}

}  // namespace ae::render

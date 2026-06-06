#include "ae/render/debug_renderer.h"

#include "ae/core/log.h"
#include "ae/render/font_atlas.h"
#include "ae/render/humanoid_mesh.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

namespace ae::render {
namespace {

FontAtlas& shared_ui_font_atlas() {
    static FontAtlas atlas;
    static bool attempted_init = false;
    if (!attempted_init) {
        attempted_init = true;
        atlas.initialize_default();
    }
    return atlas;
}

constexpr float kPi = 3.14159265358979323846F;

struct Mat4 {
    std::array<float, 16> values {};
};

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

Mat4 perspective(float vertical_fov_radians, float aspect_ratio, float near_plane, float far_plane) {
    const float f = 1.0F / std::tan(vertical_fov_radians * 0.5F);

    Mat4 result {};
    result.values[0] = f / aspect_ratio;
    result.values[5] = f;
    result.values[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.values[11] = -1.0F;
    result.values[14] = (2.0F * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

Mat4 look_at(Vec3 eye, Vec3 target, Vec3 world_up) {
    const Vec3 forward = normalize(subtract(target, eye));
    const Vec3 side = normalize(cross(world_up, forward));
    const Vec3 up = cross(forward, side);

    Mat4 result {};
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

void set_color(float red, float green, float blue) {
    glColor3f(red, green, blue);
}

void draw_line(Vec3 from, Vec3 to) {
    glVertex3f(from.x, from.y, from.z);
    glVertex3f(to.x, to.y, to.z);
}

void draw_ground_grid(int half_extent, float spacing, float brightness = 1.0F) {
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

void draw_debug_map() {
    // ====== Javelin-4 inspired Crucible arena ======
    // Symmetrical Vex-themed map with central structure,
    // outer ring, connector bridges, and spawn areas.

    // --- Arena floor (large dark ground plate) ---
    set_color(0.10F, 0.13F, 0.18F);
    draw_box({-15.0F, -0.05F, -15.0F}, {15.0F, 0.0F, 15.0F});

    // --- Outer ring track (ground level) ---
    // Ring-like ground area around the central platform
    set_color(0.14F, 0.17F, 0.22F);
    draw_box({-8.0F, 0.0F, -8.0F}, {8.0F, 0.05F, 8.0F});

    // --- Central octagonal platform (B point) ---
    // Raised 1.5m, ~4m radius
    set_color(0.22F, 0.28F, 0.36F);
    draw_box({-4.0F, 0.0F, -4.0F}, {4.0F, 1.5F, 4.0F});

    // Central pillar (Vex timegate structure)
    set_color(0.26F, 0.32F, 0.40F);
    draw_box({-0.8F, 1.5F, -0.8F}, {0.8F, 3.5F, 0.8F});

    // --- 4 Ramps from ring to central platform (N/S/E/W) ---
    set_color(0.18F, 0.24F, 0.32F);
    // North ramp
    glBegin(GL_QUADS);
    glNormal3f(0, 0.7F, 0.7F);
    glVertex3f(-1.5F, 0.05F, 4.0F); glVertex3f(1.5F, 0.05F, 4.0F);
    glVertex3f(1.5F, 1.5F, 4.0F); glVertex3f(-1.5F, 1.5F, 4.0F);
    // South ramp
    glVertex3f(-1.5F, 0.05F, -4.0F); glVertex3f(1.5F, 0.05F, -4.0F);
    glVertex3f(1.5F, 1.5F, -4.0F); glVertex3f(-1.5F, 1.5F, -4.0F);
    glEnd();

    // East ramp
    set_color(0.17F, 0.23F, 0.31F);
    glBegin(GL_QUADS);
    glNormal3f(0.7F, 0.7F, 0);
    glVertex3f(4.0F, 0.05F, -1.5F); glVertex3f(4.0F, 0.05F, 1.5F);
    glVertex3f(4.0F, 1.5F, 1.5F); glVertex3f(4.0F, 1.5F, -1.5F);
    // West ramp
    glVertex3f(-4.0F, 0.05F, -1.5F); glVertex3f(-4.0F, 0.05F, 1.5F);
    glVertex3f(-4.0F, 1.5F, 1.5F); glVertex3f(-4.0F, 1.5F, -1.5F);
    glEnd();

    // --- Cover blocks on central platform ---
    set_color(0.28F, 0.34F, 0.42F);
    draw_box({2.0F, 1.5F, 0.6F}, {3.2F, 2.2F, 1.4F});   // NE pillar
    draw_box({-3.2F, 1.5F, 0.6F}, {-2.0F, 2.2F, 1.4F});  // NW pillar
    draw_box({2.0F, 1.5F, -1.4F}, {3.2F, 2.2F, -0.6F});  // SE pillar
    draw_box({-3.2F, 1.5F, -1.4F}, {-2.0F, 2.2F, -0.6F}); // SW pillar

    // --- 4 Connector bridges (45° angles, elevated at Y=1.0) ---
    auto draw_bridge = [&](float cx, float cz, float dx, float dz) {
        const float hw = 1.0F;
        const float y = 1.0F;
        const float thick = 0.15F;
        set_color(0.16F, 0.32F, 0.28F);
        // Bridge surface (oriented box between arena ring and outer area)
        float len = 5.0F;
        float nx = -dz / len * hw;
        float nz = dx / len * hw;
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(cx + dx*2.0F - nx, y + thick, cz + dz*2.0F - nz);
        glVertex3f(cx + dx*2.0F + nx, y + thick, cz + dz*2.0F + nz);
        glVertex3f(cx + dx*7.0F + nx, y + thick, cz + dz*7.0F + nz);
        glVertex3f(cx + dx*7.0F - nx, y + thick, cz + dz*7.0F - nz);
        glEnd();

        // Bridge supports
        set_color(0.13F, 0.26F, 0.22F);
        draw_box({cx + dx*4.0F - 0.2F, 0.0F, cz + dz*4.0F - 0.2F},
                 {cx + dx*4.0F + 0.2F, y, cz + dz*4.0F + 0.2F});
    };

    draw_bridge(4.0F, 4.0F, 0.707F, 0.707F);    // NE bridge
    draw_bridge(-4.0F, 4.0F, -0.707F, 0.707F);  // NW bridge
    draw_bridge(4.0F, -4.0F, 0.707F, -0.707F);  // SE bridge
    draw_bridge(-4.0F, -4.0F, -0.707F, -0.707F); // SW bridge

    // --- Alpha Spawn (West, X = -12) ---
    set_color(0.18F, 0.23F, 0.30F);
    draw_box({-13.0F, 0.0F, -3.0F}, {-10.0F, 0.3F, 3.0F});
    // Spawn cover
    set_color(0.24F, 0.30F, 0.36F);
    draw_box({-13.0F, 0.3F, 0.0F}, {-11.0F, 1.5F, 1.5F});
    draw_box({-13.0F, 0.3F, -1.5F}, {-11.0F, 1.5F, 0.0F});

    // --- Bravo Spawn (East, X = +12) ---
    set_color(0.18F, 0.23F, 0.30F);
    draw_box({10.0F, 0.0F, -3.0F}, {13.0F, 0.3F, 3.0F});
    // Spawn cover
    set_color(0.24F, 0.30F, 0.36F);
    draw_box({11.0F, 0.3F, 0.0F}, {13.0F, 1.5F, 1.5F});
    draw_box({11.0F, 0.3F, -1.5F}, {13.0F, 1.5F, 0.0F});

    // --- Heavy ammo alcoves (North and South) ---
    set_color(0.20F, 0.24F, 0.28F);
    draw_box({-2.0F, 0.0F, 8.0F}, {2.0F, 0.3F, 9.5F});   // North heavy
    draw_box({-2.0F, 0.0F, -9.5F}, {2.0F, 0.3F, -8.0F});  // South heavy
    set_color(0.26F, 0.30F, 0.36F);
    draw_box({-1.0F, 0.3F, 8.5F}, {1.0F, 1.0F, 9.2F});    // North ammo cover
    draw_box({-1.0F, 0.3F, -9.2F}, {1.0F, 1.0F, -8.5F});   // South ammo cover

    // --- Side route platforms (between bridges, Y = 0.8) ---
    set_color(0.17F, 0.27F, 0.25F);
    draw_box({5.0F, 0.8F, 6.0F}, {7.0F, 1.0F, 8.0F});     // E side route
    draw_box({-7.0F, 0.8F, 6.0F}, {-5.0F, 1.0F, 8.0F});   // W side route
    draw_box({5.0F, 0.8F, -8.0F}, {7.0F, 1.0F, -6.0F});   // E side route S
    draw_box({-7.0F, 0.8F, -8.0F}, {-5.0F, 1.0F, -6.0F}); // W side route S

    // --- Scattered cover blocks on outer ring ---
    set_color(0.28F, 0.34F, 0.38F);
    draw_box({5.5F, 0.0F, 2.5F}, {6.5F, 0.9F, 3.5F});
    draw_box({-6.5F, 0.0F, 2.5F}, {-5.5F, 0.9F, 3.5F});
    draw_box({5.5F, 0.0F, -3.5F}, {6.5F, 0.9F, -2.5F});
    draw_box({-6.5F, 0.0F, -3.5F}, {-5.5F, 0.9F, -2.5F});
    draw_box({2.5F, 0.0F, 5.5F}, {3.5F, 0.9F, 6.5F});
    draw_box({-3.5F, 0.0F, 5.5F}, {-2.5F, 0.9F, 6.5F});
    draw_box({2.5F, 0.0F, -6.5F}, {3.5F, 0.9F, -5.5F});
    draw_box({-3.5F, 0.0F, -6.5F}, {-2.5F, 0.9F, -5.5F});

    // --- Low boundary walls ---
    set_color(0.20F, 0.24F, 0.30F);
    draw_box({-14.0F, 0.0F, -14.2F}, {14.0F, 0.4F, -13.8F}); // South wall
    draw_box({-14.0F, 0.0F, 13.8F}, {14.0F, 0.4F, 14.2F});  // North wall
    draw_box({-14.2F, 0.0F, -14.0F}, {-13.8F, 0.4F, 14.0F}); // West wall
    draw_box({13.8F, 0.0F, -14.0F}, {14.2F, 0.4F, 14.0F});  // East wall

    // --- Direction markers on ground ---
    set_color(0.8F, 0.2F, 0.2F);  // Red = North
    glLineWidth(2.0F);
    glBegin(GL_LINES);
    draw_line({0.0F, 0.06F, 8.5F}, {0.0F, 0.06F, 11.0F});
    draw_line({-0.5F, 0.06F, 10.5F}, {0.0F, 0.06F, 11.0F});
    draw_line({0.5F, 0.06F, 10.5F}, {0.0F, 0.06F, 11.0F});
    glEnd();
    set_color(0.2F, 0.2F, 0.8F);  // Blue = South
    glBegin(GL_LINES);
    draw_line({0.0F, 0.06F, -8.5F}, {0.0F, 0.06F, -11.0F});
    draw_line({-0.5F, 0.06F, -10.5F}, {0.0F, 0.06F, -11.0F});
    draw_line({0.5F, 0.06F, -10.5F}, {0.0F, 0.06F, -11.0F});
    glEnd();
    glLineWidth(1.0F);
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
        set_color(0.5F, 0.5F, 0.5F);
        glBegin(GL_QUADS);
        // +Z face
        glNormal3f(0, 0, 1);
        glVertex3f(bx - hs, foot_y, bz + hs); glVertex3f(bx + hs, foot_y, bz + hs); glVertex3f(bx + hs, top, bz + hs); glVertex3f(bx - hs, top, bz + hs);
        // -Z face
        glNormal3f(0, 0, -1);
        glVertex3f(bx + hs, foot_y, bz - hs); glVertex3f(bx - hs, foot_y, bz - hs); glVertex3f(bx - hs, top, bz - hs); glVertex3f(bx + hs, top, bz - hs);
        // -X face
        glNormal3f(-1, 0, 0);
        glVertex3f(bx - hs, foot_y, bz - hs); glVertex3f(bx - hs, foot_y, bz + hs); glVertex3f(bx - hs, top, bz + hs); glVertex3f(bx - hs, top, bz - hs);
        // +X face
        glNormal3f(1, 0, 0);
        glVertex3f(bx + hs, foot_y, bz + hs); glVertex3f(bx + hs, foot_y, bz - hs); glVertex3f(bx + hs, top, bz - hs); glVertex3f(bx + hs, top, bz + hs);
        // +Y face
        glNormal3f(0, 1, 0);
        glVertex3f(bx - hs, top, bz + hs); glVertex3f(bx + hs, top, bz + hs); glVertex3f(bx + hs, top, bz - hs); glVertex3f(bx - hs, top, bz - hs);
        // -Y face
        glNormal3f(0, -1, 0);
        glVertex3f(bx - hs, foot_y, bz - hs); glVertex3f(bx + hs, foot_y, bz - hs); glVertex3f(bx + hs, foot_y, bz + hs); glVertex3f(bx - hs, foot_y, bz + hs);
        glEnd();
    }

    // Legs — two lines from hips to feet, rotated
    {
        const float leg_offset = 0.05F;
        const auto [lx, lz] = rotate_xz(-leg_offset, 0.0F);
        const auto [rx, rz] = rotate_xz(leg_offset, 0.0F);
        glLineWidth(3.0F);
        glBegin(GL_LINES);
        set_color(0.2F, 0.35F, 0.55F);
        draw_line({position.x + lx, hips_y, position.z + lz}, {position.x + lx, foot_y, position.z + lz});
        draw_line({position.x + rx, hips_y, position.z + rz}, {position.x + rx, foot_y, position.z + rz});
        glEnd();
        glLineWidth(1.0F);
    }

    // Torso — oriented box from hips to shoulders
    {
        set_color(1.0F, 0.82F, 0.18F);
        const float bx = position.x;
        const float bz = position.z;
        const float hw = body_half_width;
        const float hd = body_half_depth;

        auto [fl_x, fl_z] = rotate_xz(-hw, hd);
        auto [fr_x, fr_z] = rotate_xz(hw, hd);
        auto [bl_x, bl_z] = rotate_xz(-hw, -hd);
        auto [br_x, br_z] = rotate_xz(hw, -hd);

        // Rotated face normals for the torso
        const auto [nf_x, nf_z] = rotate_xz(0.0F, 1.0F);
        const auto [nb_x, nb_z] = rotate_xz(0.0F, -1.0F);
        const auto [nl_x, nl_z] = rotate_xz(-1.0F, 0.0F);
        const auto [nr_x, nr_z] = rotate_xz(1.0F, 0.0F);

        glBegin(GL_QUADS);
        // Front face
        glNormal3f(nf_x, 0, nf_z);
        glVertex3f(bx + fl_x, hips_y, bz + fl_z); glVertex3f(bx + fr_x, hips_y, bz + fr_z); glVertex3f(bx + fr_x, shoulders_y, bz + fr_z); glVertex3f(bx + fl_x, shoulders_y, bz + fl_z);
        // Back face
        glNormal3f(nb_x, 0, nb_z);
        glVertex3f(bx + br_x, hips_y, bz + br_z); glVertex3f(bx + bl_x, hips_y, bz + bl_z); glVertex3f(bx + bl_x, shoulders_y, bz + bl_z); glVertex3f(bx + br_x, shoulders_y, bz + br_z);
        // Left face
        glNormal3f(nl_x, 0, nl_z);
        glVertex3f(bx + bl_x, hips_y, bz + bl_z); glVertex3f(bx + fl_x, hips_y, bz + fl_z); glVertex3f(bx + fl_x, shoulders_y, bz + fl_z); glVertex3f(bx + bl_x, shoulders_y, bz + bl_z);
        // Right face
        glNormal3f(nr_x, 0, nr_z);
        glVertex3f(bx + fr_x, hips_y, bz + fr_z); glVertex3f(bx + br_x, hips_y, bz + br_z); glVertex3f(bx + br_x, shoulders_y, bz + br_z); glVertex3f(bx + fr_x, shoulders_y, bz + fr_z);
        // Top face
        glNormal3f(0, 1, 0);
        glVertex3f(bx + fl_x, shoulders_y, bz + fl_z); glVertex3f(bx + fr_x, shoulders_y, bz + fr_z); glVertex3f(bx + br_x, shoulders_y, bz + br_z); glVertex3f(bx + bl_x, shoulders_y, bz + bl_z);
        // Bottom face
        glNormal3f(0, -1, 0);
        glVertex3f(bx + bl_x, hips_y, bz + bl_z); glVertex3f(bx + br_x, hips_y, bz + br_z); glVertex3f(bx + fr_x, hips_y, bz + fr_z); glVertex3f(bx + fl_x, hips_y, bz + fl_z);
        glEnd();
    }

    // Arms — two lines from shoulders, direction-aware
    {
        const float shoulder_off_x = body_half_width + 0.02F;
        const float arm_length = height * 0.35F;
        const float hand_drop = arm_length * 0.85F;
        const float hand_side = arm_length * 0.3F;

        const auto [lsx, lsz] = rotate_xz(-shoulder_off_x, 0.0F);
        const auto [rsx, rsz] = rotate_xz(shoulder_off_x, 0.0F);
        const auto [lex, lez] = rotate_xz(-shoulder_off_x - hand_side, -hand_drop);
        const auto [rex, rez] = rotate_xz(shoulder_off_x + hand_side, -hand_drop);

        glLineWidth(2.5F);
        glBegin(GL_LINES);
        set_color(1.0F, 0.65F, 0.0F);
        draw_line({position.x + lsx, shoulders_y, position.z + lsz},
                  {position.x + lex, shoulders_y + hand_drop, position.z + lez});
        draw_line({position.x + rsx, shoulders_y, position.z + rsz},
                  {position.x + rex, shoulders_y + hand_drop, position.z + rez});
        glEnd();
        glLineWidth(1.0F);
    }

    // Head — octahedron, rotated to face yaw
    {
        set_color(1.0F, 0.88F, 0.65F);
        const float cx = position.x;
        const float cy = head_center_y;
        const float cz = position.z;
        const float r = head_radius;

        auto [fx, fz] = rotate_xz(0.0F, r);
        auto [bx2, bz2] = rotate_xz(0.0F, -r);
        auto [lx2, lz2] = rotate_xz(-r, 0.0F);
        auto [rx2, rz2] = rotate_xz(r, 0.0F);

        const Vec3 top    {cx,           cy + r, cz};
        const Vec3 bottom {cx,           cy - r, cz};
        const Vec3 front2 {cx + fx,      cy,     cz + fz};
        const Vec3 back2  {cx + bx2,     cy,     cz + bz2};
        const Vec3 left2  {cx + lx2,     cy,     cz + lz2};
        const Vec3 right2 {cx + rx2,     cy,     cz + rz2};

        glBegin(GL_TRIANGLES);
        // Top-front-right
        glNormal3f(0.577F, 0.577F, 0.577F);
        glVertex3f(top.x,    top.y,    top.z);    glVertex3f(front2.x, front2.y, front2.z); glVertex3f(right2.x, right2.y, right2.z);
        // Top-right-back
        glNormal3f(0.577F, 0.577F, -0.577F);
        glVertex3f(top.x,    top.y,    top.z);    glVertex3f(right2.x, right2.y, right2.z); glVertex3f(back2.x,  back2.y,  back2.z);
        // Top-back-left
        glNormal3f(-0.577F, 0.577F, -0.577F);
        glVertex3f(top.x,    top.y,    top.z);    glVertex3f(back2.x,  back2.y,  back2.z);  glVertex3f(left2.x,  left2.y,  left2.z);
        // Top-left-front
        glNormal3f(-0.577F, 0.577F, 0.577F);
        glVertex3f(top.x,    top.y,    top.z);    glVertex3f(left2.x,  left2.y,  left2.z);  glVertex3f(front2.x, front2.y, front2.z);

        // Bottom-front-right
        glNormal3f(0.577F, -0.577F, 0.577F);
        glVertex3f(bottom.x, bottom.y, bottom.z); glVertex3f(right2.x, right2.y, right2.z); glVertex3f(front2.x, front2.y, front2.z);
        // Bottom-right-back
        glNormal3f(0.577F, -0.577F, -0.577F);
        glVertex3f(bottom.x, bottom.y, bottom.z); glVertex3f(back2.x,  back2.y,  back2.z);  glVertex3f(right2.x, right2.y, right2.z);
        // Bottom-back-left
        glNormal3f(-0.577F, -0.577F, -0.577F);
        glVertex3f(bottom.x, bottom.y, bottom.z); glVertex3f(left2.x,  left2.y,  left2.z);  glVertex3f(back2.x,  back2.y,  back2.z);
        // Bottom-left-front
        glNormal3f(-0.577F, -0.577F, 0.577F);
        glVertex3f(bottom.x, bottom.y, bottom.z); glVertex3f(front2.x, front2.y, front2.z); glVertex3f(left2.x,  left2.y,  left2.z);
        glEnd();
    }
}

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

void draw_text(float x, float y, float scale, const std::string& text);

enum class UiTextStyle {
    Header,
    Section,
    Body,
    Accent,
    Muted,
    Inverted
};

void draw_ui_text(float x, float y, float scale, const std::string& text, UiTextStyle style);

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

void draw_text(float x, float y, float scale, const std::string& text) {
    FontAtlas& atlas = shared_ui_font_atlas();
    if (atlas.is_ready()) {
        atlas.draw_text(x, y, scale, text);
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

std::string format_overlay_line(const char* label, double value, const char* suffix = "") {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.1f%s", label, value, suffix);
    return buffer;
}

std::string format_integer_overlay_line(const char* label, double value, const char* suffix = "") {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f%s", label, std::floor(value), suffix);
    return buffer;
}

std::string format_memory_line(const char* label, double used_mb, double total_mb) {
    char buffer[128] {};
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f/%.0fMB", label, used_mb, total_mb);
    return buffer;
}

void draw_metrics_overlay(const DebugScene& scene, int width, int height) {
    const std::array<std::string, 10> lines {{
        std::string("MODE: ") + scene.camera_mode_name,
        format_integer_overlay_line("FPS", scene.fps),
        format_integer_overlay_line("1%% LOW", scene.fps_p1_low),
        format_integer_overlay_line("1%% HIGH", scene.fps_p1_high),
        format_integer_overlay_line("FRAME", scene.frame_time_ms, " MS"),
        format_overlay_line("PROC CPU", scene.process_cpu_percent, "%"),
        format_overlay_line("SYS CPU", scene.system_cpu_percent, "%"),
        format_integer_overlay_line("RSS", scene.process_rss_mb, " MB"),
        format_memory_line("SYS MEM", scene.system_used_memory_mb, scene.system_total_memory_mb),
        scene.gpu_usage_available ? format_overlay_line("GPU", scene.gpu_usage_percent, "%") : std::string("GPU: N/A")
    }};

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

    glColor4f(0.04F, 0.06F, 0.09F, 0.92F);
    glBegin(GL_QUADS);
    draw_screen_quad(16.0F, 16.0F, 348.0F, 260.0F);
    glEnd();

    glColor4f(0.16F, 0.28F, 0.44F, 1.0F);
    glBegin(GL_QUADS);
    draw_screen_quad(16.0F, 16.0F, 348.0F, 28.0F);
    glEnd();

    draw_ui_text(28.0F, 24.0F, 2.0F, "METRICS", UiTextStyle::Header);
    float y = 56.0F;
    for (const auto& line : lines) {
        draw_ui_text(28.0F, y, 3.0F, line, UiTextStyle::Section);
        y += 24.0F;
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void draw_crosshair_overlay(int width, int height) {
    const float center_x = static_cast<float>(width) * 0.5F;
    const float center_y = static_cast<float>(height) * 0.5F;
    constexpr float arm_length = 8.0F;
    constexpr float gap = 4.0F;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0F);
    glColor3f(1.0F, 0.9F, 0.1F);  // Bright yellow crosshair
    glBegin(GL_LINES);
    glVertex2f(center_x - arm_length, center_y);
    glVertex2f(center_x - gap, center_y);
    glVertex2f(center_x + gap, center_y);
    glVertex2f(center_x + arm_length, center_y);
    glVertex2f(center_x, center_y - arm_length);
    glVertex2f(center_x, center_y - gap);
    glVertex2f(center_x, center_y + gap);
    glVertex2f(center_x, center_y + arm_length);
    glEnd();
    glLineWidth(1.0F);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void draw_hud(const DebugScene& scene, int width, int height, float hud_brightness) {
    // --- Screen-space setup (health bar + ammo) ---
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

    // --- Health Bar (bottom-center) ---
    constexpr float hb_w = 320.0F;
    constexpr float hb_h = 10.0F;
    const float hb_x = static_cast<float>(width) * 0.5F - hb_w * 0.5F;
    const float hb_y = static_cast<float>(height) - 45.0F;

    float health_pct = scene.player_max_health > 0.0F
                           ? scene.player_health / scene.player_max_health
                           : 0.0F;
    if (health_pct > 1.0F) health_pct = 1.0F;
    if (health_pct < 0.0F) health_pct = 0.0F;

    const bool low_health = health_pct <= 0.3F;
    float pulse = 1.0F;
    if (low_health) {
        pulse = 0.6F + 0.4F * std::sin(static_cast<float>(glfwGetTime()) * 12.0F);
    }

    // HP Label (Top-Left)
    draw_ui_text(hb_x, hb_y - 15.0F, 1.5F, "HP", low_health ? UiTextStyle::Accent : UiTextStyle::Header);

    // Numeric value (Top-Right)
    char health_buf[32]{};
    std::snprintf(health_buf, sizeof(health_buf), "%.0f / %.0f",
                  static_cast<double>(scene.player_health),
                  static_cast<double>(scene.player_max_health));
    FontAtlas& font_atlas = shared_ui_font_atlas();
    const float value_w = font_atlas.measure_text(health_buf, 1.4F);
    draw_ui_text(hb_x + hb_w - value_w, hb_y - 15.0F, 1.4F, health_buf, low_health ? UiTextStyle::Accent : UiTextStyle::Header);

    // Draw Health Bar Background Panel (Glassmorphic dark slate)
    const float hb_alpha = 0.65F * hud_brightness;
    draw_panel(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.04F, 0.06F, 0.09F, hb_alpha);
    if (low_health) {
        draw_panel_outline(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.85F, 0.15F * pulse, 0.15F * pulse, 0.9F * hud_brightness);
    } else {
        draw_panel_outline(hb_x - 3.0F, hb_y - 3.0F, hb_w + 6.0F, hb_h + 6.0F, 0.22F, 0.34F, 0.52F, 0.75F * hud_brightness);
    }

    // Segmented health bar fill
    if (health_pct > 0.0F) {
        float bar_r = 0.2F, bar_g = 0.85F, bar_b = 0.2F;
        if (health_pct <= 0.3F) {
            bar_r = 0.85F; bar_g = 0.15F * pulse; bar_b = 0.15F * pulse;
        } else if (health_pct <= 0.6F) {
            bar_r = 0.85F; bar_g = 0.85F; bar_b = 0.15F;
        }

        constexpr int num_segments = 20;
        const float segment_spacing = 2.0F;
        const float total_spacing_w = segment_spacing * (num_segments - 1);
        const float segment_w = (hb_w - total_spacing_w) / num_segments;

        glColor4f(bar_r, bar_g, bar_b, 0.9F * hud_brightness);
        glBegin(GL_QUADS);
        for (int i = 0; i < num_segments; ++i) {
            const float seg_x = hb_x + static_cast<float>(i) * (segment_w + segment_spacing);
            const float seg_mid = (static_cast<float>(i) + 0.5F) / static_cast<float>(num_segments);
            if (seg_mid <= health_pct) {
                draw_screen_quad(seg_x, hb_y, segment_w, hb_h);
            }
        }
        glEnd();
    }

    draw_xbox_button_legend(scene.controller_buttons, hb_x + hb_w + 24.0F, hb_y - 30.0F);

    // --- Ammo Counter (bottom-left card) ---
    const float card_x = 24.0F;
    const float card_w = 190.0F;
    const float card_h = 76.0F;
    const float card_y = static_cast<float>(height) - card_h - 24.0F;

    float ammo_pct = scene.ammo_max > 0.0F ? scene.ammo_current / scene.ammo_max : 0.0F;
    if (ammo_pct > 1.0F) ammo_pct = 1.0F;
    if (ammo_pct < 0.0F) ammo_pct = 0.0F;

    const bool low_ammo = scene.ammo_current <= 5.0F;
    float ammo_pulse = 1.0F;
    if (low_ammo) {
        ammo_pulse = 0.6F + 0.4F * std::sin(static_cast<float>(glfwGetTime()) * 10.0F);
    }

    const float card_alpha = 0.65F * hud_brightness;
    draw_panel(card_x, card_y, card_w, card_h, 0.04F, 0.06F, 0.09F, card_alpha);
    if (low_ammo) {
        draw_panel_outline(card_x, card_y, card_w, card_h, 0.85F, 0.15F * ammo_pulse, 0.15F * ammo_pulse, 0.85F * hud_brightness);
    } else {
        draw_panel_outline(card_x, card_y, card_w, card_h, 0.96F, 0.84F, 0.16F, 0.7F * hud_brightness);
    }

    char curr_ammo_buf[16]{};
    std::snprintf(curr_ammo_buf, sizeof(curr_ammo_buf), "%.0f", static_cast<double>(scene.ammo_current));
    draw_ui_text(card_x + 16.0F, card_y + 12.0F, 3.4F, curr_ammo_buf, low_ammo ? UiTextStyle::Accent : UiTextStyle::Header);

    char reserve_buf[16]{};
    std::snprintf(reserve_buf, sizeof(reserve_buf), "/ %.0f", static_cast<double>(scene.ammo_max));
    draw_ui_text(card_x + 72.0F, card_y + 24.0F, 1.8F, reserve_buf, UiTextStyle::Muted);

    draw_ui_text(card_x + 16.0F, card_y + 52.0F, 1.1F, "STANDARD RIFLE", UiTextStyle::Muted);

    const float mag_bar_x = card_x + 16.0F;
    const float mag_bar_y = card_y + 44.0F;
    const float mag_bar_w = card_w - 32.0F;
    const float mag_bar_h = 3.0F;

    draw_panel(mag_bar_x, mag_bar_y, mag_bar_w, mag_bar_h, 0.1F, 0.12F, 0.15F, 1.0F * hud_brightness);
    if (ammo_pct > 0.0F) {
        if (low_ammo) {
            glColor4f(0.85F, 0.15F * ammo_pulse, 0.15F * ammo_pulse, 0.9F * hud_brightness);
        } else {
            glColor4f(0.96F, 0.84F, 0.16F, 0.9F * hud_brightness);
        }
        glBegin(GL_QUADS);
        draw_screen_quad(mag_bar_x, mag_bar_y, mag_bar_w * ammo_pct, mag_bar_h);
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // --- Minimap (bottom-right, separate world-space ortho) ---
    constexpr float mm_size = 150.0F;
    constexpr float mm_margin = 10.0F;
    const int mm_x = width - static_cast<int>(mm_size) - static_cast<int>(mm_margin);
    const int mm_y = height - static_cast<int>(mm_size) - static_cast<int>(mm_margin);

    glViewport(mm_x, mm_y, static_cast<int>(mm_size), static_cast<int>(mm_size));

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // Map 30x30m arena (-15..15) to circular radar
    constexpr double world_extent = 16.0;
    glOrtho(-world_extent, world_extent, world_extent, -world_extent, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dark semi-transparent background circle
    constexpr float r_inner = 14.5F;
    glColor4f(0.02F, 0.04F, 0.08F, 0.75F);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0.0F, 0.0F);
    for (int step = 0; step <= 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();

    // Arena boundary outline (white)
    glColor3f(0.6F, 0.6F, 0.6F);
    glLineWidth(1.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-15.0F, -15.0F);
    glVertex2f(15.0F, -15.0F);
    glVertex2f(15.0F, 15.0F);
    glVertex2f(-15.0F, 15.0F);
    glEnd();

    // Central platform (blue square, +/-4)
    glColor3f(0.18F, 0.38F, 0.78F);
    glBegin(GL_QUADS);
    glVertex2f(-4.0F, -4.0F);
    glVertex2f(4.0F, -4.0F);
    glVertex2f(4.0F, 4.0F);
    glVertex2f(-4.0F, 4.0F);
    glEnd();

    // Alpha Spawn (gray, X=-13..-10, Z=-3..3)
    glColor3f(0.35F, 0.35F, 0.35F);
    glBegin(GL_QUADS);
    glVertex2f(-13.0F, -3.0F);
    glVertex2f(-10.0F, -3.0F);
    glVertex2f(-10.0F, 3.0F);
    glVertex2f(-13.0F, 3.0F);
    glEnd();

    // Bravo Spawn (gray, X=10..13, Z=-3..3)
    glBegin(GL_QUADS);
    glVertex2f(10.0F, -3.0F);
    glVertex2f(13.0F, -3.0F);
    glVertex2f(13.0F, 3.0F);
    glVertex2f(10.0F, 3.0F);
    glEnd();

    // Draw projectiles on the radar (tactical orange pings)
    for (int i = 0; i < scene.projectile_count && i < 64; ++i) {
        const auto& pp = scene.projectile_positions[i];
        const float dist = std::sqrt(pp.x * pp.x + pp.z * pp.z);
        if (dist < r_inner) {
            glColor4f(1.0F, 0.55F, 0.1F, 0.85F);
            glPointSize(4.0F);
            glBegin(GL_POINTS);
            glVertex2f(pp.x, pp.z);
            glEnd();
        }
    }
    glPointSize(1.0F);

    // Radar sweep line with fade trailing
    const float sweep_angle = -static_cast<float>(glfwGetTime()) * 1.5F;
    glBegin(GL_LINES);
    for (int i = 0; i < 5; ++i) {
        float angle = sweep_angle + static_cast<float>(i) * 0.05F;
        float alpha = 0.4F * (1.0F - static_cast<float>(i) / 5.0F);
        glColor4f(0.0F, 1.0F, 0.4F, alpha);
        glVertex2f(0.0F, 0.0F);
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();

    // Mask out the corners of the viewport to make the minimap circular
    glColor4f(0.05F, 0.07F, 0.11F, 1.0F);
    glBegin(GL_QUAD_STRIP);
    constexpr float r_outer = 30.0F;
    for (int step = 0; step <= 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        const float cos_a = std::cos(angle);
        const float sin_a = std::sin(angle);
        glVertex2f(cos_a * r_inner, sin_a * r_inner);
        glVertex2f(cos_a * r_outer, sin_a * r_outer);
    }
    glEnd();

    // High-tech circular outer border
    glColor4f(0.22F, 0.5F, 0.85F, 0.8F);
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    for (int step = 0; step < 36; ++step) {
        const float angle = static_cast<float>(step) / 36.0F * 2.0F * kPi;
        glVertex2f(std::cos(angle) * r_inner, std::sin(angle) * r_inner);
    }
    glEnd();
    glLineWidth(1.0F);

    // Player directional arrow
    const float yaw = scene.player_yaw;
    const float arrow_len = 1.6F;
    const float arrow_width = 1.0F;
    float dx = std::sin(yaw);
    float dz = std::cos(yaw);
    float tx = -dz;
    float tz = dx;

    float px = scene.player_position.x;
    float pz = scene.player_position.z;
    float arrow_pulse = 0.8F + 0.2F * std::sin(static_cast<float>(glfwGetTime()) * 5.0F);

    glColor4f(0.0F, 1.0F, 0.3F, arrow_pulse);
    glBegin(GL_TRIANGLES);
    glVertex2f(px + dx * arrow_len, pz + dz * arrow_len);
    glVertex2f(px - dx * 0.8F + tx * arrow_width, pz - dz * 0.8F + tz * arrow_width);
    glVertex2f(px - dx * 0.8F - tx * arrow_width, pz - dz * 0.8F - tz * arrow_width);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Restore full viewport
    glViewport(0, 0, width, height);

    // Draw Compass letters in screen-space overlay (N, S, E, W)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    const float mm_cx = static_cast<float>(mm_x) + mm_size * 0.5F;
    const float mm_cy = static_cast<float>(mm_y) + mm_size * 0.5F;
    draw_ui_text(mm_cx - 4.0F, mm_cy - 74.0F, 1.1F, "N", UiTextStyle::Header);
    draw_ui_text(mm_cx - 4.0F, mm_cy + 63.0F, 1.1F, "S", UiTextStyle::Header);
    draw_ui_text(mm_cx - 72.0F, mm_cy - 5.0F, 1.1F, "W", UiTextStyle::Header);
    draw_ui_text(mm_cx + 64.0F, mm_cy - 5.0F, 1.1F, "E", UiTextStyle::Header);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void draw_menu_overlay(const DebugScene& scene, int width, int height) {
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

    draw_panel(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height), 0.0F, 0.0F, 0.0F, 0.72F);

    const float panel_x = 90.0F;
    const float panel_y = 72.0F;
    const float panel_w = static_cast<float>(width) - 180.0F;
    const float panel_h = static_cast<float>(height) - 144.0F;
    const float sidebar_w = 210.0F;
    const float content_x = panel_x + sidebar_w + 24.0F;
    const float content_y = panel_y + 84.0F;

    draw_panel(panel_x, panel_y, panel_w, panel_h, 0.04F, 0.06F, 0.09F, 0.96F);
    draw_panel_outline(panel_x, panel_y, panel_w, panel_h, 0.18F, 0.30F, 0.48F, 0.9F);
    draw_panel(panel_x, panel_y, panel_w, 54.0F, 0.10F, 0.18F, 0.30F, 1.0F);
    draw_panel(panel_x, panel_y, sidebar_w, panel_h, 0.05F, 0.08F, 0.12F, 0.96F);

    draw_ui_text(panel_x + 24.0F, panel_y + 16.0F, 2.6F, "PLAYER MENU", UiTextStyle::Header);
    draw_ui_text(panel_x + panel_w - 170.0F, panel_y + 16.0F, 1.8F, "START: CLOSE", UiTextStyle::Muted);

    const char* tabs[] = {"CHARACTER", "SETTINGS"};
    for (int i = 0; i < 2; ++i) {
        const float tab_y = panel_y + 86.0F + static_cast<float>(i) * 52.0F;
        if (i == scene.menu_tab) {
            draw_panel(panel_x + 14.0F, tab_y - 10.0F, sidebar_w - 28.0F, 38.0F, 0.16F, 0.28F, 0.44F, 1.0F);
            draw_ui_text(panel_x + 34.0F, tab_y, 2.2F, tabs[i], UiTextStyle::Header);
        } else {
            draw_ui_text(panel_x + 34.0F, tab_y, 2.2F, tabs[i], UiTextStyle::Muted);
        }
    }

    draw_ui_text(panel_x + 28.0F, panel_y + panel_h - 54.0F, 1.8F, "LB / RB SWITCH TABS", UiTextStyle::Muted);
    draw_ui_text(panel_x + 28.0F, panel_y + panel_h - 30.0F, 1.8F, "BACK = METRICS", UiTextStyle::Muted);

    if (scene.menu_tab == 0) {
        draw_panel(content_x, content_y, panel_w - sidebar_w - 48.0F, 170.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y, panel_w - sidebar_w - 48.0F, 170.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 16.0F, 2.4F, "LOADOUT", UiTextStyle::Header);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "HP %.0f / %.0f", scene.player_health, scene.player_max_health);
        glColor3f(0.68F, 0.88F, 0.40F);
        draw_text(content_x + 24.0F, content_y + 54.0F, 2.2F, buf);

        std::snprintf(buf, sizeof(buf), "AMMO %.0f / %.0f", scene.ammo_current, scene.ammo_max);
        glColor3f(0.96F, 0.84F, 0.16F);
        draw_text(content_x + 24.0F, content_y + 84.0F, 2.2F, buf);

        draw_panel(content_x, content_y + 194.0F, panel_w - sidebar_w - 48.0F, 220.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 194.0F, panel_w - sidebar_w - 48.0F, 220.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 210.0F, 2.4F, "GEAR", UiTextStyle::Section);
        draw_ui_text(content_x + 24.0F, content_y + 248.0F, 2.1F, "PRIMARY   STANDARD RIFLE", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 280.0F, 2.1F, "SPECIAL   SHOTGUN", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 312.0F, 2.1F, "HEAVY     ROCKET", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 356.0F, 1.9F, "MOBILITY  75", UiTextStyle::Body);
        draw_ui_text(content_x + 160.0F, content_y + 356.0F, 1.9F, "RESILIENCE 62", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 386.0F, 1.9F, "RECOVERY  58", UiTextStyle::Body);

        draw_panel(content_x + panel_w - sidebar_w - 220.0F, content_y + 24.0F, 140.0F, 140.0F, 0.11F, 0.15F, 0.22F, 1.0F);
        draw_panel_outline(content_x + panel_w - sidebar_w - 220.0F, content_y + 24.0F, 140.0F, 140.0F, 0.30F, 0.40F, 0.56F, 0.9F);
        draw_ui_text(content_x + panel_w - sidebar_w - 186.0F, content_y + 74.0F, 2.4F, "GUARDIAN", UiTextStyle::Section);
    } else {
        draw_panel(content_x, content_y, panel_w - sidebar_w - 48.0F, 150.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y, panel_w - sidebar_w - 48.0F, 150.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 16.0F, 2.4F, "GRAPHICS", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 52.0F, 2.0F, "RESOLUTION   1920 X 1080", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 82.0F, 2.0F, "FULLSCREEN   OFF", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 112.0F, 2.0F, "FOV          90", UiTextStyle::Body);

        draw_panel(content_x, content_y + 174.0F, panel_w - sidebar_w - 48.0F, 126.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 174.0F, panel_w - sidebar_w - 48.0F, 126.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 190.0F, 2.4F, "AUDIO", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 226.0F, 2.0F, "MASTER      80", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 256.0F, 2.0F, "SFX         90", UiTextStyle::Body);

        draw_panel(content_x, content_y + 324.0F, panel_w - sidebar_w - 48.0F, 190.0F, 0.08F, 0.11F, 0.16F, 0.92F);
        draw_panel_outline(content_x, content_y + 324.0F, panel_w - sidebar_w - 48.0F, 190.0F, 0.22F, 0.34F, 0.52F, 0.85F);
        draw_ui_text(content_x + 20.0F, content_y + 340.0F, 2.4F, "CONTROLS", UiTextStyle::Header);
        draw_ui_text(content_x + 24.0F, content_y + 376.0F, 2.0F, "MOVE        LEFT STICK", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 406.0F, 2.0F, "LOOK        RIGHT STICK", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 436.0F, 2.0F, "JUMP        A", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 466.0F, 2.0F, "FIRE        RT", UiTextStyle::Body);
        draw_ui_text(content_x + 24.0F, content_y + 496.0F, 2.0F, "RELOAD      Y", UiTextStyle::Body);

        draw_xbox_button_legend(scene.controller_buttons, content_x + panel_w - sidebar_w - 292.0F, content_y + 348.0F);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace

struct DebugRenderer::Impl {
    GLFWwindow* window {nullptr};
};

DebugRenderer::DebugRenderer() = default;

DebugRenderer::~DebugRenderer() {
    shutdown();
}

// No need for manual delete — unique_ptr handles it

bool DebugRenderer::initialize(ae::PlatformWindow& window) {
    if (impl_ != nullptr) {
        return true;
    }

    auto* glfw_window = static_cast<GLFWwindow*>(window.native_handle());
    if (glfw_window == nullptr) {
        log_error("DebugRenderer received an invalid platform window handle.");
        return false;
    }

    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Ambient light — minimum floor so unlit faces are never pitch black,
    // even at "night".  These get overridden per-frame based on time of day.
    GLfloat ambient[] = {0.20F, 0.24F, 0.30F, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    // Directional sun (warm, from upper-right-front)
    GLfloat light_pos[] = {0.5F, 1.0F, 0.8F, 0.0F}; // w=0 means directional
    GLfloat light_diffuse[] = {0.85F, 0.82F, 0.78F, 1.0F};
    GLfloat light_specular[] = {0.3F, 0.3F, 0.3F, 1.0F};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // Moonlight fill (cool blue, opposite direction) — ensures shadowed
    // faces still receive some directional light even at night.
    glEnable(GL_LIGHT1);
    GLfloat moon_pos[] = {-0.5F, 0.6F, -0.8F, 0.0F};
    GLfloat moon_diffuse[] = {0.0F, 0.0F, 0.0F, 1.0F};  // off by default, set per-frame
    GLfloat moon_specular[] = {0.0F, 0.0F, 0.0F, 1.0F};
    glLightfv(GL_LIGHT1, GL_POSITION, moon_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, moon_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, moon_specular);

    glClearColor(0.03F, 0.035F, 0.045F, 1.0F);

    impl_ = std::make_unique<Impl>();
    impl_->window = glfw_window;

    log_info("DebugRenderer initialized with GLFW/OpenGL debug backend.");
    return true;
}

void DebugRenderer::shutdown() {
    if (impl_ == nullptr) {
        return;
    }

    glfwMakeContextCurrent(nullptr);
    impl_.reset();
}

void DebugRenderer::render(const DebugScene& scene) {
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
    // Night sky is a dark navy blue (not pitch black) so the horizon is
    // always visible and objects can be silhouetted against it.
    float night_r = 0.08F, night_g = 0.10F, night_b = 0.18F;
    float day_r = 0.35F, day_g = 0.55F, day_b = 0.85F;
    float cr = night_r + (day_r - night_r) * (float)day_factor;
    float cg = night_g + (day_g - night_g) * (float)day_factor;
    float cb = night_b + (day_b - night_b) * (float)day_factor;
    glClearColor(cr, cg, cb, 1.0F);

    int framebuffer_width = 1;
    int framebuffer_height = 1;
    glfwGetFramebufferSize(impl_->window, &framebuffer_width, &framebuffer_height);
    const int width = framebuffer_width > 0 ? framebuffer_width : 1;
    const int height = framebuffer_height > 0 ? framebuffer_height : 1;
    const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        // Zenith colour: darker than horizon for a natural sky look
        float zenith_r = cr * 0.35F;
        float zenith_g = cg * 0.35F;
        float zenith_b = cb * 0.55F;
        // Only draw the top third — rest is already cleard to horizon colour
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

        glEnable(GL_DEPTH_TEST);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    const Mat4 projection = perspective(60.0F * kPi / 180.0F, aspect_ratio, 0.05F, 250.0F);
    const Mat4 view = look_at(scene.camera_position, scene.camera_target, {0.0F, 1.0F, 0.0F});

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.values.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.values.data());

    glDisable(GL_LIGHTING);
    draw_ground_grid(40, 1.0F, (float)day_factor);
    draw_axes();
    glEnable(GL_LIGHTING);

    // Update ambient and diffuse light based on time of day.
    // Ambient has a minimum floor so the scene is never completely dark —
    // even at "night" there's enough ambient to navigate and see geometry.
    // Gamma is a user-configurable brightness multiplier (default 1.0).
    float gamma = scene.gamma > 0.0F ? scene.gamma : 1.0F;
    float light_brightness = (0.30F + 0.70F * (float)day_factor) * gamma;
    constexpr float kMinAmbientR = 0.06F;
    constexpr float kMinAmbientG = 0.08F;
    constexpr float kMinAmbientB = 0.12F;
    float ambient_r = (kMinAmbientR + (0.18F - kMinAmbientR) * (float)day_factor) * gamma;
    float ambient_g = (kMinAmbientG + (0.22F - kMinAmbientG) * (float)day_factor) * gamma;
    float ambient_b = (kMinAmbientB + (0.28F - kMinAmbientB) * (float)day_factor) * gamma;
    GLfloat ambient[] = {ambient_r, ambient_g, ambient_b, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    // Sun diffuse: fades from full brightness down to a dim twilight level
    GLfloat diffuse[] = {0.95F * light_brightness, 0.9F * light_brightness, 0.8F * light_brightness, 1.0F};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

    // Moonlight fill: inversely proportional to sunlight — strongest at night,
    // fades to nothing during the day.  Cool blue tint for visual contrast.
    float moonlight = 1.0F - (float)day_factor;
    float moon_strength = 0.25F * moonlight;
    GLfloat moon_diffuse[] = {0.08F * moon_strength, 0.14F * moon_strength, 0.28F * moon_strength, 1.0F};
    glLightfv(GL_LIGHT1, GL_DIFFUSE, moon_diffuse);

    draw_debug_map();
    if (scene.show_player_marker) {
        // Procedural humanoid mesh with per-part colors
        static const GltfModel kHumanoid = generate_humanoid_mesh();
        const float s = scene.player_height;
        const float cy = std::cos(scene.player_yaw);
        const float sy = std::sin(scene.player_yaw);
        const float px = scene.player_position.x;
        const float py = scene.player_position.y;
        const float pz = scene.player_position.z;
        for (const auto& m : kHumanoid.meshes) {
            if (m.positions.empty()) continue;
            const std::size_t vc = m.positions.size() / 3;
            const auto* idx = m.indices.empty() ? nullptr : m.indices.data();
            const std::size_t tc = idx ? m.indices.size() / 3 : vc / 3;
            set_color(m.color_r, m.color_g, m.color_b);
            glBegin(GL_TRIANGLES);
            for (std::size_t t = 0; t < tc; ++t) {
                for (int j = 0; j < 3; ++j) {
                    const std::size_t vi = idx ? static_cast<std::size_t>(idx[t * 3 + j]) : t * 3 + j;
                    if (vi >= vc) continue;
                    const float lx = m.positions[vi * 3 + 0] * s;
                    const float ly = m.positions[vi * 3 + 1] * s;
                    const float lz = m.positions[vi * 3 + 2] * s;
                    glVertex3f(px + lx * cy - lz * sy, py + ly, pz + lx * sy + lz * cy);
                }
            }
            glEnd();
        }
    }

    if (scene.show_crosshair) {
        draw_crosshair_overlay(width, height);
    }
    if (scene.metrics_visible) {
        draw_metrics_overlay(scene, width, height);
    }

    // Draw projectiles
    glDisable(GL_LIGHTING);
    for (int i = 0; i < scene.projectile_count && i < 64; ++i) {
        const auto& pp = scene.projectile_positions[i];
        set_color(1.0F, 0.55F, 0.1F);
        const float r = 0.08F;
        glBegin(GL_TRIANGLES);
        // Simple octahedron
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

    draw_hud(scene, width, height, (float)day_factor);

    if (scene.menu_visible) {
        draw_menu_overlay(scene, width, height);
    }

    glfwSwapBuffers(impl_->window);
}

}  // namespace ae::render

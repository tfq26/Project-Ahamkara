#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace ae::physics {

struct Vec3 {
    float x {0.0F};
    float y {0.0F};
    float z {0.0F};
};

using BodyHandle = std::uint32_t;
using CharacterHandle = std::uint32_t;
constexpr BodyHandle kInvalidBody = 0xFFFFFFFF;
constexpr CharacterHandle kInvalidCharacter = 0xFFFFFFFF;

enum class BodyType {
    Static,
    Kinematic,
    Dynamic
};

struct BodyDesc {
    Vec3 half_extents {0.5F, 0.5F, 0.5F};
    Vec3 position {};
    BodyType type {BodyType::Static};
    bool is_sensor {false};  // no collision response, just overlap events
};

struct CapsuleDesc {
    float half_height {0.5F};
    float radius {0.25F};
    Vec3 position {};
};

struct CharacterDesc {
    float radius {0.22F};
    float height {1.70F};    // total height (capsule tip to tip)
    Vec3 position {};
    float max_slope_degrees {50.0F};
    float mass {70.0F};
};

struct RayResult {
    bool hit {false};
    Vec3 point {};
    Vec3 normal {};
    float distance {0.0F};
    BodyHandle body {kInvalidBody};
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Bodies
    BodyHandle create_box_body(const BodyDesc& desc);
    BodyHandle create_capsule_body(const CapsuleDesc& desc, BodyType type = BodyType::Static);
    void destroy_body(BodyHandle handle);
    void set_body_position(BodyHandle handle, const Vec3& position);
    Vec3 get_body_position(BodyHandle handle) const;
    void set_body_velocity(BodyHandle handle, const Vec3& velocity);

    // Character controller
    CharacterHandle create_character(const CharacterDesc& desc);
    void destroy_character(CharacterHandle handle);
    void set_character_velocity(CharacterHandle handle, const Vec3& velocity);
    Vec3 get_character_position(CharacterHandle handle) const;
    Vec3 get_character_velocity(CharacterHandle handle) const;
    bool character_on_ground(CharacterHandle handle) const;
    void set_character_position(CharacterHandle handle, const Vec3& position);
    void set_character_shape(CharacterHandle handle, float radius, float height);

    // Raycast (against static + kinematic bodies)
    RayResult raycast(const Vec3& origin, const Vec3& direction, float max_distance) const;

    // Contact filtering — set a body as jump-through (character can pass from below)
    void set_body_jump_through(BodyHandle handle, bool enabled);

    // Simulation
    void tick(float delta_seconds);

    // Debug — get all body positions for rendering
    struct BodyDebugInfo {
        BodyHandle handle;
        Vec3 position;
        Vec3 half_extents;
        bool is_static;
    };
    std::vector<BodyDebugInfo> get_body_debug_info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ae::physics

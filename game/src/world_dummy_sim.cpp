#include "world_dummy_sim.h"
#include "world_jolt_bridge.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <cmath>
#include <algorithm>

namespace ahamkara::game {

void tick_dummies(
    entt::registry& registry,
    TargetDummyState* dummies,
    int dummy_count,
    ae::u32 current_tick,
    float delta_seconds) {

    (void)current_tick;

    for (int i = 0; i < dummy_count; ++i) {
        auto& d = dummies[i];

        // Respawn timer
        if (!d.alive) {
            d.respawn_timer -= delta_seconds;
            if (d.respawn_timer <= 0.0F) {
                d.alive = true;
                d.health = 100.0F;
                d.position = d.start_position;
                d.last_hit_timer = 0.0F;
            }
            continue;
        }

        // Hit feedback timer
        if (d.last_hit_timer > 0.0F) {
            d.last_hit_timer = std::max(0.0F, d.last_hit_timer - delta_seconds);
        }

        // Movement: oscillate along move_dir
        if (d.move_speed > 0.0F && d.move_distance > 0.0F) {
            d.move_timer += delta_seconds * d.move_speed;
            float offset = std::sin(d.move_timer) * d.move_distance;
            d.position.x = d.start_position.x + d.move_dir.x * offset;
            d.position.z = d.start_position.z + d.move_dir.z * offset;
        }
    }

    // Sync EnTT components
    auto view = registry.view<TargetDummyComponent>();
    int idx = 0;
    for (auto entity : view) {
        if (idx >= dummy_count) break;
        auto& comp = view.get<TargetDummyComponent>(entity);
        comp.state = dummies[idx];
        ++idx;
    }
}

void sync_dummies_to_jolt(
    JPH::PhysicsSystem& physics_system,
    std::vector<JPH::BodyID>& dummy_bodies,
    const TargetDummyState* dummies,
    int dummy_count) {

    auto& bi = physics_system.GetBodyInterface();

    // Ensure we have enough bodies
    while ((int)dummy_bodies.size() < dummy_count) {
        JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.35f);
        capsule_settings.SetEmbedded();
        JPH::ShapeSettings::ShapeResult shape_result = capsule_settings.Create();
        if (!shape_result.IsValid()) break;

        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3(0.0f, 0.0f, 0.0f),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            Layers::MOVING
        );

        JPH::BodyID body_id = bi.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
        dummy_bodies.push_back(body_id);
    }

    for (int i = 0; i < dummy_count && i < (int)dummy_bodies.size(); ++i) {
        const auto& d = dummies[i];
        if (!d.alive) {
            // Move dead dummies far below the map
            bi.SetPosition(dummy_bodies[i], JPH::RVec3(0.0f, -100.0f, 0.0f), JPH::EActivation::DontActivate);
        } else {
            bi.SetPosition(dummy_bodies[i],
                JPH::RVec3(d.position.x, d.position.y, d.position.z),
                JPH::EActivation::DontActivate);
        }
    }
}

}  // namespace ahamkara::game

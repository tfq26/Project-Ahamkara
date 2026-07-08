#include "ahamkara/client/debug_scene_bridge.h"

#include "ae/core/math.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/weapon_registry.h"
#include "ahamkara/client/weapon_viewmodel_data.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kFirstPersonTargetDistance = 12.0F;
constexpr float kDebugFollowDistance = 4.5F;
constexpr float kDebugFollowLift = 1.8F;
constexpr float kDebugFollowShoulderOffset = 0.75F;
constexpr float kDebugFollowLeadDistance = 1.5F;

[[nodiscard]] ae::Vec3 camera_forward_from_anchor(const ahamkara::game::CameraAnchor& anchor) {
    const float yaw_radians = ae::to_radians(anchor.yaw);
    const float pitch_radians = ae::to_radians(anchor.pitch);
    const float cos_pitch = std::cos(pitch_radians);

    return {
        std::sin(yaw_radians) * cos_pitch,
        std::sin(pitch_radians),
        std::cos(yaw_radians) * cos_pitch,
    };
}

[[nodiscard]] ae::Vec3 horizontal_forward_from_anchor(const ahamkara::game::CameraAnchor& anchor) {
    ae::Vec3 forward = camera_forward_from_anchor(anchor);
    forward.y = 0.0F;

    if (forward.length_squared() <= ae::epsilon) {
        return {0.0F, 0.0F, 1.0F};
    }

    return forward.normalized();
}

[[nodiscard]] ae::render::Vec3 to_render_vec3(const ae::Vec3& value) {
    return {value.x, value.y, value.z};
}

void add_level_mesh_debug_boxes(
    ae::render::DebugScene& scene,
    const ae::render::LevelAsset& level_asset) {
    for (const auto& mesh : level_asset.mesh_instances) {
        if (scene.level_box_count >= 64) {
            break;
        }

        const float half_x = std::max(0.1F, mesh.scale_x * 0.5F);
        const float half_y = std::max(0.1F, mesh.scale_y * 0.5F);
        const float half_z = std::max(0.1F, mesh.scale_z * 0.5F);
        auto& box = scene.level_boxes[scene.level_box_count++];
        box.min = {mesh.pos_x - half_x, mesh.pos_y - half_y, mesh.pos_z - half_z};
        box.max = {mesh.pos_x + half_x, mesh.pos_y + half_y, mesh.pos_z + half_z};
        box.red = 0.95F;
        box.green = 0.62F;
        box.blue = 0.18F;
    }
}

}  // namespace

namespace ahamkara::client {

ae::render::DebugScene build_debug_scene(
    const ClientSimulationSnapshot& previous_snapshot,
    const ClientSimulationSnapshot& current_snapshot,
    const DebugSceneBuildInputs& inputs) {
    // Camera smoothing state stays local to the bridge so the renderer-facing
    // scene contract remains stable even if we refine camera behavior later.
    static ae::Vec3 smooth_eye_pos {0, 2, -5};
    static ae::Vec3 smooth_target_pos {0, 1, 0};
    static bool smooth_initialized = false;

    const auto& player_state = current_snapshot.player_state;
    const float player_height = current_snapshot.player_height;
    const float alpha = inputs.alpha;

    const ae::Vec3 player_position {
        previous_snapshot.player_position.x + (current_snapshot.player_position.x - previous_snapshot.player_position.x) * alpha,
        previous_snapshot.player_position.y + (current_snapshot.player_position.y - previous_snapshot.player_position.y) * alpha,
        previous_snapshot.player_position.z + (current_snapshot.player_position.z - previous_snapshot.player_position.z) * alpha
    };

    ahamkara::game::CameraAnchor anchor;
    anchor.position = {
        previous_snapshot.camera_anchor.position.x + (current_snapshot.camera_anchor.position.x - previous_snapshot.camera_anchor.position.x) * alpha,
        previous_snapshot.camera_anchor.position.y + (current_snapshot.camera_anchor.position.y - previous_snapshot.camera_anchor.position.y) * alpha,
        previous_snapshot.camera_anchor.position.z + (current_snapshot.camera_anchor.position.z - previous_snapshot.camera_anchor.position.z) * alpha
    };
    float diff_yaw = ae::wrap_degrees(current_snapshot.camera_anchor.yaw - previous_snapshot.camera_anchor.yaw);
    anchor.yaw = previous_snapshot.camera_anchor.yaw + diff_yaw * alpha;
    float diff_pitch = ae::wrap_degrees(current_snapshot.camera_anchor.pitch - previous_snapshot.camera_anchor.pitch);
    anchor.pitch = previous_snapshot.camera_anchor.pitch + diff_pitch * alpha;

    const ae::Vec3 player_center {
        player_position.x,
        player_position.y + player_height * 0.5F,
        player_position.z,
    };
    const ae::Vec3 anchor_position {anchor.position.x, anchor.position.y, anchor.position.z};
    const ae::Vec3 world_up {0.0F, 1.0F, 0.0F};
    const ae::Vec3 forward = camera_forward_from_anchor(anchor).normalized();
    const auto normalize_or = [](const ae::Vec3& value, const ae::Vec3& fallback) {
        return value.length_squared() <= ae::epsilon ? fallback : value.normalized();
    };
    const ae::Vec3 view_right = normalize_or(ae::cross(world_up, forward), {1.0F, 0.0F, 0.0F});
    const ae::Vec3 view_up = normalize_or(ae::cross(forward, view_right), world_up);

    ae::render::DebugScene scene {};
    scene.player_position = to_render_vec3(player_position);
    scene.player_height = player_height;
    scene.player_yaw = ae::to_radians(anchor.yaw);
    scene.player_health = player_state.health;
    scene.player_max_health = 100.0F;
    scene.ammo_current = current_snapshot.ammo_current;
    scene.ammo_max = current_snapshot.ammo_max;
    scene.weapon_index = current_snapshot.weapon_index;
    scene.reserve_ammo = current_snapshot.reserve_ammo;
    scene.weapon_name = ahamkara::game::weapon_name(current_snapshot.weapon_index);

    // Per-weapon viewmodel offsets — populated from the canonical presentation
    // data in weapon_viewmodel_data.h.  These are stored in the client layer
    // and remain fully decoupled from gameplay/weapon-runtime code.
    {
        const int wi = current_snapshot.weapon_index;
        const auto vm = weapon_viewmodel_transform(wi);
        scene.viewmodel_position_offset = {vm.pos_right, vm.pos_up, vm.pos_forward};
        scene.viewmodel_fov_scale = vm.fov_scale;
        scene.viewmodel_pitch_deg = vm.pitch_deg;
        scene.viewmodel_yaw_deg   = vm.yaw_deg;
        scene.viewmodel_roll_deg  = vm.roll_deg;
    }
    scene.always_day = inputs.always_day;
    scene.menu_visible = inputs.menu_visible;
    scene.menu_tab = inputs.menu_tab;
    scene.gamma = inputs.gamma;
    scene.match_time = current_snapshot.match_time;
    scene.match_phase = current_snapshot.match_phase;
    scene.team_score_red = current_snapshot.team_score_red;
    scene.team_score_blue = current_snapshot.team_score_blue;
    if (inputs.level_asset != nullptr) {
        add_level_mesh_debug_boxes(scene, *inputs.level_asset);
    }

    scene.hitmarker_time = current_snapshot.hitmarker_time;
    scene.hitmarker_is_critical = current_snapshot.hitmarker_is_critical;
    scene.muzzle_flash_time = current_snapshot.muzzle_flash_time;

    const int hit_number_count = current_snapshot.damage_number_count;
    scene.hit_number_count = hit_number_count;
    for (int i = 0; i < hit_number_count && i < 16; ++i) {
        const auto& damage_number = current_snapshot.damage_numbers[i];
        scene.hit_number_positions[i] = {
            damage_number.position.x, damage_number.position.y, damage_number.position.z};
        scene.hit_number_values[i] = damage_number.value;
        scene.hit_number_is_critical[i] = damage_number.is_critical;
        scene.hit_number_lifetimes[i] = damage_number.lifetime;
    }

    const int dummy_count = current_snapshot.dummy_count;
    scene.dummy_count = dummy_count;
    for (int i = 0; i < dummy_count && i < 16; ++i) {
        const auto& current_dummy = current_snapshot.dummies[i];
        const auto& previous_dummy = previous_snapshot.dummies[i];

        ae::render::Vec3 dummy_position {
            previous_dummy.position.x + (current_dummy.position.x - previous_dummy.position.x) * alpha,
            previous_dummy.position.y + (current_dummy.position.y - previous_dummy.position.y) * alpha,
            previous_dummy.position.z + (current_dummy.position.z - previous_dummy.position.z) * alpha
        };
        float diff_dummy_yaw = ae::wrap_degrees(current_dummy.yaw - previous_dummy.yaw);
        float dummy_yaw = previous_dummy.yaw + diff_dummy_yaw * alpha;

        scene.dummy_positions[i] = dummy_position;
        scene.dummy_yaws[i] = ae::to_radians(dummy_yaw);
        scene.dummy_alive[i] = current_dummy.alive;
        scene.dummy_recently_hit[i] = (current_dummy.last_hit_timer > 0.0F);
    }

    scene.enemy_count = dummy_count;
    for (int i = 0; i < dummy_count && i < 16; ++i) {
        scene.enemy_health[i] = current_snapshot.enemy_health[i];
        scene.enemy_max_health[i] = current_snapshot.enemy_max_health[i];
    }

    const int projectile_count = current_snapshot.projectile_count;
    scene.projectile_count = projectile_count;
    for (int i = 0; i < projectile_count && i < 64; ++i) {
        const auto& projectile = current_snapshot.projectiles[i];
        if (projectile.alive) {
            scene.projectile_positions[i] = {
                projectile.position.x, projectile.position.y, projectile.position.z};
        }
    }

    const int particle_count = current_snapshot.particle_count;
    scene.particle_count = particle_count;
    for (int i = 0; i < particle_count && i < 256; ++i) {
        const auto& particle = current_snapshot.particles[i];
        if (particle.alive) {
            scene.particle_positions[i] = {particle.position.x, particle.position.y, particle.position.z};
            scene.particle_sizes[i] = particle.size;
            scene.particle_colors_r[i] = particle.r;
            scene.particle_colors_g[i] = particle.g;
            scene.particle_colors_b[i] = particle.b;
            scene.particle_alphas[i] =
                particle.max_lifetime > 0.0F ? particle.lifetime_seconds / particle.max_lifetime : 0.0F;
        }
    }

    const int decal_count = current_snapshot.decal_count;
    scene.decal_count = decal_count;
    for (int i = 0; i < decal_count && i < 64; ++i) {
        const auto& decal = current_snapshot.decals[i];
        if (decal.alive) {
            scene.decal_positions[i] = {decal.position.x, decal.position.y, decal.position.z};
            scene.decal_normals[i] = {decal.normal.x, decal.normal.y, decal.normal.z};
            scene.decal_sizes[i] = decal.size;
        }
    }

    // Budget/pacing fields — populated regardless of metrics visibility
    scene.frame_budget_ms = inputs.frame_budget_ms;
    scene.frame_p1_low_ms = inputs.frame_p1_low_ms;
    scene.frame_rolling_avg_ms = inputs.frame_rolling_avg_ms;
    scene.frame_budget_compliance = inputs.frame_budget_compliance;
    scene.frame_pacing_healthy = inputs.frame_pacing_healthy;
    scene.frame_regression = inputs.frame_regression;
    scene.rss_pressure = inputs.rss_pressure;
    scene.rss_bytes = inputs.rss_bytes;
    scene.rss_peak_bytes = inputs.rss_peak_bytes;
    scene.rss_soft_budget = inputs.rss_soft_budget;
    scene.rss_hard_budget = inputs.rss_hard_budget;
    scene.frame_alloc_pressure = inputs.frame_alloc_pressure;
    scene.frame_alloc_peak_bytes = inputs.frame_alloc_peak_bytes;
    scene.frame_alloc_capacity_bytes = inputs.frame_alloc_capacity_bytes;

    if (inputs.displayed_metrics != nullptr) {
        scene.metrics_visible = inputs.metrics_visible;
        scene.gpu_profiler_visible = inputs.gpu_profiler_visible;
        scene.fps = inputs.displayed_metrics->fps;
        scene.frame_time_ms = inputs.displayed_metrics->frame_time_ms;
        scene.fps_p1_low = inputs.displayed_metrics->fps_p1_low;
        scene.fps_p1_high = inputs.displayed_metrics->fps_p1_high;
        scene.process_cpu_percent = inputs.displayed_metrics->process_cpu_percent;
        scene.system_cpu_percent = inputs.displayed_metrics->system_cpu_percent;
        scene.process_rss_mb = inputs.displayed_metrics->process_rss_mb;
        scene.system_used_memory_mb = inputs.displayed_metrics->system_used_memory_mb;
        scene.system_total_memory_mb = inputs.displayed_metrics->system_total_memory_mb;
        scene.gpu_usage_available = inputs.displayed_metrics->gpu_usage_available;
        scene.gpu_usage_percent = inputs.displayed_metrics->gpu_usage_percent;
    }
    scene.camera_mode_name = camera_mode_name(inputs.camera_mode);

    if (inputs.camera_mode == CameraMode::FirstPerson) {
        scene.camera_position = to_render_vec3(anchor_position);
        scene.camera_target = to_render_vec3(anchor_position + forward * kFirstPersonTargetDistance);
        scene.viewmodel_position = to_render_vec3(anchor_position);
        scene.viewmodel_forward = to_render_vec3(forward);
        scene.viewmodel_right = to_render_vec3(view_right);
        scene.viewmodel_up = to_render_vec3(view_up);
        scene.show_player_marker = false;
        scene.show_crosshair = true;
        return scene;
    }

    const ae::Vec3 horizontal_forward = horizontal_forward_from_anchor(anchor);
    const ae::Vec3 right = ae::cross(world_up, horizontal_forward).normalized();
    const ae::Vec3 eye = anchor_position
        - horizontal_forward * kDebugFollowDistance
        + world_up * kDebugFollowLift
        + right * kDebugFollowShoulderOffset;
    const ae::Vec3 target = player_center + horizontal_forward * kDebugFollowLeadDistance;

    const float lerp_factor = std::min(1.0F, 15.0F * (1.0F / 60.0F));
    if (!smooth_initialized) {
        smooth_eye_pos = eye;
        smooth_target_pos = target;
        smooth_initialized = true;
    } else {
        smooth_eye_pos.x += (eye.x - smooth_eye_pos.x) * lerp_factor;
        smooth_eye_pos.y += (eye.y - smooth_eye_pos.y) * lerp_factor;
        smooth_eye_pos.z += (eye.z - smooth_eye_pos.z) * lerp_factor;
        smooth_target_pos.x += (target.x - smooth_target_pos.x) * lerp_factor;
        smooth_target_pos.y += (target.y - smooth_target_pos.y) * lerp_factor;
        smooth_target_pos.z += (target.z - smooth_target_pos.z) * lerp_factor;
    }

    scene.camera_position = to_render_vec3(smooth_eye_pos);
    scene.camera_target = to_render_vec3(smooth_target_pos);
    scene.viewmodel_position = to_render_vec3(anchor_position);
    scene.viewmodel_forward = to_render_vec3(forward);
    scene.viewmodel_right = to_render_vec3(view_right);
    scene.viewmodel_up = to_render_vec3(view_up);
    scene.show_player_marker = true;
    scene.show_crosshair = false;
    return scene;
}

}  // namespace ahamkara::client

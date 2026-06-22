#include "ahamkara/client/debug_render_runtime.h"

#include "ae/core/math.h"

namespace ahamkara::client {

DebugRenderSubmission build_debug_render_submission(
    const ClientSimulationSnapshot& current_snapshot,
    const ae::GamepadState& gamepad,
    const ae::render::DebugScene& base_scene) {
    DebugRenderSubmission submission;
    submission.scene = base_scene;
    submission.scene.controller_buttons = 0;

    if (gamepad.connected) {
        for (int i = 0; i < static_cast<int>(ae::kGamepadButtonCount); ++i) {
            if (gamepad.is_button_down(static_cast<ae::GamepadButton>(i))) {
                submission.scene.controller_buttons |= (1u << i);
            }
        }
    }

    const auto& anchor = current_snapshot.camera_anchor;
    const float yaw_radians = ae::to_radians(anchor.yaw);
    const float pitch_radians = ae::to_radians(anchor.pitch);
    submission.listener_position[0] = anchor.position.x;
    submission.listener_position[1] = anchor.position.y;
    submission.listener_position[2] = anchor.position.z;
    submission.listener_forward[0] = std::sin(yaw_radians) * std::cos(pitch_radians);
    submission.listener_forward[1] = -std::sin(pitch_radians);
    submission.listener_forward[2] = std::cos(yaw_radians) * std::cos(pitch_radians);

    const float cam_pos[3] = {anchor.position.x, anchor.position.y, anchor.position.z};
    const float view_m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, -cam_pos[0],-cam_pos[1],-cam_pos[2],1};
    const float proj_m[16] = {1.3f,0,0,0, 0,1.7f,0,0, 0,0,-1.002f,-1, 0,0,-0.2f,0};
    for (int i = 0; i < 16; ++i) {
        submission.shadow_view[i] = view_m[i];
        submission.shadow_projection[i] = proj_m[i];
    }

    submission.shadow_caster_count = std::min(base_scene.level_box_count, 64);
    for (int i = 0; i < submission.shadow_caster_count; ++i) {
        const auto& box = base_scene.level_boxes[i];
        submission.shadow_casters[i].min[0] = box.min.x;
        submission.shadow_casters[i].min[1] = box.min.y;
        submission.shadow_casters[i].min[2] = box.min.z;
        submission.shadow_casters[i].max[0] = box.max.x;
        submission.shadow_casters[i].max[1] = box.max.y;
        submission.shadow_casters[i].max[2] = box.max.z;
    }

    return submission;
}

void render_local_debug_frame(
    const DebugRenderSubmission& submission,
    ae::audio::AudioEngine& audio_engine,
    ae::render::ShadowPass& shadow_pass,
    ae::render::DebugRenderer& renderer,
    ae::render::PbrRenderer& pbr_renderer,
    const ae::render::LevelRenderScene* level_scene) {
    audio_engine.set_listener(
        submission.listener_position[0], submission.listener_position[1], submission.listener_position[2],
        submission.listener_forward[0], submission.listener_forward[1], submission.listener_forward[2],
        submission.listener_up[0], submission.listener_up[1], submission.listener_up[2]);

    shadow_pass.begin_pass(submission.shadow_view, submission.shadow_projection);
    for (int i = 0; i < submission.shadow_caster_count; ++i) {
        shadow_pass.submit_box_caster(submission.shadow_casters[i]);
    }
    shadow_pass.end_pass();

    auto scene = submission.scene;

    // Draw level mesh instances through the PBR path via a world-phase hook so
    // they render before DebugRenderer's screen-space overlays (HUD, crosshair,
    // menus) and cannot overwrite them. Uses the camera matrices the debug
    // renderer computes this frame so meshes align with the world.
    if (level_scene != nullptr && !level_scene->empty()) {
        renderer.render(scene, [&]() {
            if (ae::render::RenderBackend* backend = renderer.backend()) {
                backend->set_depth_test(true);
                backend->set_depth_write(true);
                backend->set_depth_func_lequal();
            }
            pbr_renderer.begin_frame(renderer.view_matrix(), renderer.projection_matrix(),
                                     renderer.camera_position(), &shadow_pass);
            level_scene->submit(pbr_renderer);
            pbr_renderer.end_frame();
        });
    } else {
        renderer.render(scene);
    }
}

}  // namespace ahamkara::client

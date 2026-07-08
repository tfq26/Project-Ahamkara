#pragma once

#include "ae/audio/audio_engine.h"
#include "ae/platform/gamepad.h"
#include "ae/render/debug_renderer.h"
#include "ae/render/level_render.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/shadow_pass.h"
#include "ahamkara/client/debug_scene_bridge.h"

namespace ahamkara::client {

struct DebugRenderSubmission {
    ae::render::DebugScene scene {};
    float listener_position[3] {0.0F, 0.0F, 0.0F};
    float listener_forward[3] {0.0F, 0.0F, 1.0F};
    float listener_up[3] {0.0F, 1.0F, 0.0F};
    float shadow_view[16] {};
    float shadow_projection[16] {};
    int shadow_caster_count {0};
    ae::render::ShadowBoxCaster shadow_casters[64] {};
};

[[nodiscard]] DebugRenderSubmission build_debug_render_submission(
    const ClientSimulationSnapshot& current_snapshot,
    const ae::GamepadState& gamepad,
    const ae::render::DebugScene& base_scene);

void render_local_debug_frame(
    const DebugRenderSubmission& submission,
    ae::audio::AudioEngine& audio_engine,
    ae::render::ShadowPass& shadow_pass,
    ae::render::DebugRenderer& renderer,
    ae::render::PbrRenderer& pbr_renderer,
    const ae::render::LevelRenderScene* level_scene);

}  // namespace ahamkara::client

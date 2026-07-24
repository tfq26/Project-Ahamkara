/// Unit tests for ae::animation state machine, animation graph, IK solver,
/// weapon animation layers, and blend spaces.
///
/// All tests run headlessly — no GPU, no window, no graphics context required.
#include "ae/animation/state_machine.h"
#include "ae/animation/animation_graph.h"
#include "ae/animation/ik.h"
#include "ae/animation/character_weapon.h"
#include "ae/animation/clip_player.h"
#include "ae/skeleton/types.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "  FAIL: " << msg << "\n";
        ++failures;
    } else {
        std::cout << "  ok: " << msg << "\n";
    }
}

bool nearly_equal(float a, float b, float eps = 1.0e-3F) {
    return std::fabs(a - b) <= eps;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

ae::skeleton::AnimationClipData make_simple_clip(
    const std::string& name,
    float tx_end = 0.0F, float ty_end = 0.0F, float tz_end = 0.0F)
{
    ae::skeleton::AnimationClipData clip;
    clip.name = name;

    // Translation channel: from (0,0,0) to (tx_end, ty_end, tz_end)
    {
        ae::skeleton::AnimationSampler sampler;
        sampler.input_times = {0.0F, 1.0F};
        sampler.output_values = {0.0F, 0.0F, 0.0F, tx_end, ty_end, tz_end};
        sampler.interpolation = "LINEAR";
        clip.samplers.push_back(sampler);

        ae::skeleton::AnimationChannel channel;
        channel.node_index = 0;
        channel.path = "translation";
        channel.sampler_index = static_cast<int>(clip.samplers.size() - 1);
        clip.channels.push_back(channel);
    }

    // Rotation channel: identity → identity (no rotation)
    {
        ae::skeleton::AnimationSampler sampler;
        sampler.input_times = {0.0F, 1.0F};
        sampler.output_values = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
        sampler.interpolation = "LINEAR";
        clip.samplers.push_back(sampler);

        ae::skeleton::AnimationChannel channel;
        channel.node_index = 0;
        channel.path = "rotation";
        channel.sampler_index = static_cast<int>(clip.samplers.size() - 1);
        clip.channels.push_back(channel);
    }

    return clip;
}

ae::skeleton::Skin make_single_joint_skin() {
    ae::skeleton::Skin skin;
    ae::skeleton::Joint joint;
    joint.name = "root";
    joint.parent_index = -1;
    joint.inverse_bind_matrix = ae::skeleton::Mat4::identity();
    skin.joints.push_back(joint);
    return skin;
}

// ── State Machine Tests ──────────────────────────────────────────────────────

void test_sm_initial_state() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");

    sm.tick(0.016F);

    check(sm.current_state_id() == "Idle", "SM initial state is Idle");
    check(!sm.is_transitioning(), "SM not transitioning initially");
}

void test_sm_trigger_transition() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");
    sm.add_transition("Idle", "Walk", "start_moving", 0.2F);

    sm.trigger("start_moving");
    sm.tick(0.016F);

    check(sm.is_transitioning(), "SM is transitioning after trigger");
    check(sm.current_state_id() == "Idle",
          "SM still in Idle at start of crossfade");
}

void test_sm_transition_completes() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");
    sm.add_transition("Idle", "Walk", "start_moving", 0.1F);

    sm.trigger("start_moving");
    // Tick past the blend duration
    for (int i = 0; i < 10; ++i) sm.tick(0.016F);

    check(sm.current_state_id() == "Walk", "SM transitions to Walk after crossfade");
    check(!sm.is_transitioning(), "SM crossfade completed");
}

void test_sm_active_clips_during_transition() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");
    sm.add_transition("Idle", "Walk", "start_moving", 0.2F);

    sm.trigger("start_moving");
    sm.tick(0.016F);

    auto clips = sm.active_clips();
    check(clips.size() == 2, "two active clips during crossfade");

    float total_weight = 0.0F;
    for (const auto& c : clips) total_weight += c.weight;
    check(nearly_equal(total_weight, 1.0F), "clip weights sum to 1.0 during crossfade");
}

void test_sm_single_clip_after_transition() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");
    sm.add_transition("Idle", "Walk", "start_moving", 0.05F);

    sm.trigger("start_moving");
    // Tick well past blend
    for (int i = 0; i < 20; ++i) sm.tick(0.016F);

    auto clips = sm.active_clips();
    check(clips.size() == 1, "single active clip after crossfade completes");
    check(clips[0].clip_name == "clip_walk", "active clip is Walk after transition");
}

void test_sm_unknown_trigger_does_nothing() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.set_initial_state("Idle");
    sm.add_transition("Idle", "Walk", "start_moving", 0.2F);

    sm.trigger("nonexistent");
    sm.tick(0.016F);

    check(sm.current_state_id() == "Idle",
          "SM stays in Idle after unknown trigger");
    check(!sm.is_transitioning(),
          "SM not transitioning after unknown trigger");
}

void test_sm_set_blend_param() {
    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.set_initial_state("Idle");

    // Just verify no crash
    sm.set_blend_param(0.5F);
    sm.set_blend_param(0.3F, 0.7F);

    check(true, "set_blend_param does not crash");
}

// ── Animation Graph Tests ───────────────────────────────────────────────────

void test_graph_evaluate_single_clip() {
    ae::animation::AnimationGraph graph;
    auto clip_data = make_simple_clip("test_clip", 1.0F, 0.0F, 0.0F);
    graph.register_clip("test_clip", &clip_data, 1.0F, true);

    auto skin = make_single_joint_skin();
    graph.set_skin(&skin);
    graph.set_parent_indices({-1});

    std::vector<ae::skeleton::Mat4> out;
    graph.evaluate_single("test_clip", 0.5F, out);

    check(out.size() == 1, "graph evaluate_single produces 1 joint matrix");
}

void test_graph_evaluate_with_state_machine() {
    ae::animation::AnimationGraph graph;
    auto idle_data = make_simple_clip("clip_idle");
    auto walk_data = make_simple_clip("clip_walk", 0.5F, 0.0F, 0.0F);
    graph.register_clip("clip_idle", &idle_data, 1.0F, true);
    graph.register_clip("clip_walk", &walk_data, 1.0F, true);

    auto skin = make_single_joint_skin();
    graph.set_skin(&skin);
    graph.set_parent_indices({-1});

    ae::animation::StateMachine sm;
    sm.add_state("Idle", "clip_idle");
    sm.add_state("Walk", "clip_walk");
    sm.set_initial_state("Idle");

    // Evaluate idle pose
    std::vector<ae::skeleton::Mat4> out;
    graph.evaluate(sm.active_clips(), 0.016F, out);
    check(out.size() == 1, "graph evaluate from SM produces joint matrix");

    // Trigger transition and evaluate blend
    sm.add_transition("Idle", "Walk", "go", 0.1F);
    sm.trigger("go");
    sm.tick(0.05F);
    graph.evaluate(sm.active_clips(), 0.016F, out);
    check(out.size() == 1, "graph evaluate during crossfade produces joint matrix");
}

// ── IK Solver Tests ─────────────────────────────────────────────────────────

void test_ik_two_bone_reachable() {
    ae::animation::IKChain chain;
    chain.root_joint = 0;
    chain.mid_joint = 1;
    chain.end_joint = 2;
    chain.bone_length_upper = 1.0F;
    chain.bone_length_lower = 1.0F;

    ae::animation::IKTarget target;
    target.target_x = 0.0F;
    target.target_y = -1.5F;  // reachable (2.0 total length, target at 1.5)
    target.target_z = 0.0F;
    target.enabled = true;
    target.weight = 1.0F;

    auto root_global = ae::skeleton::Mat4::identity();
    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);

    check(result.converged, "two-bone IK converges for reachable target");
}

void test_ik_two_bone_overextended() {
    ae::animation::IKChain chain;
    chain.root_joint = 0;
    chain.mid_joint = 1;
    chain.end_joint = 2;
    chain.bone_length_upper = 1.0F;
    chain.bone_length_lower = 1.0F;

    ae::animation::IKTarget target;
    target.target_x = 0.0F;
    target.target_y = -5.0F;  // unreachable (2.0 max length, target at 5.0)
    target.target_z = 0.0F;
    target.enabled = true;
    target.weight = 1.0F;

    auto root_global = ae::skeleton::Mat4::identity();
    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);

    // Should not produce NaN even when overextended
    check(!std::isnan(result.root_correction.qx) &&
          !std::isnan(result.root_correction.ty) &&
          !std::isnan(result.mid_correction.qw),
          "overextended IK target does not produce NaN");
}

void test_ik_two_bone_degenerate() {
    ae::animation::IKChain chain;
    chain.root_joint = 0;
    chain.mid_joint = 1;
    chain.end_joint = 2;
    chain.bone_length_upper = 0.0F;  // degenerate: zero-length bone
    chain.bone_length_lower = 1.0F;

    ae::animation::IKTarget target;
    target.target_x = 0.0F;
    target.target_y = -0.5F;
    target.target_z = 0.0F;
    target.enabled = true;
    target.weight = 1.0F;

    auto root_global = ae::skeleton::Mat4::identity();
    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);

    check(!std::isnan(result.root_correction.qx) &&
          !std::isnan(result.root_correction.ty) &&
          !std::isnan(result.mid_correction.qw),
          "degenerate IK chain does not produce NaN");
}

void test_ik_two_bone_disabled() {
    ae::animation::IKChain chain;
    chain.root_joint = 0;
    chain.mid_joint = 1;
    chain.end_joint = 2;
    chain.bone_length_upper = 1.0F;
    chain.bone_length_lower = 1.0F;

    ae::animation::IKTarget target;
    target.target_x = 0.0F;
    target.target_y = -1.5F;
    target.target_z = 0.0F;
    target.enabled = false;  // disabled
    target.weight = 0.0F;

    auto root_global = ae::skeleton::Mat4::identity();
    auto result = ae::animation::IKSolver::solve_two_bone(chain, target, root_global);

    // Disabled target with zero weight should still not crash and produce valid results
    check(!std::isnan(result.root_correction.qx),
          "disabled IK target does not produce NaN");
}

void test_ik_add_chain() {
    ae::animation::IKSolver solver;
    ae::animation::IKChain chain;
    chain.root_joint = 0;
    chain.mid_joint = 1;
    chain.end_joint = 2;

    int idx = solver.add_chain(chain);
    check(idx == 0, "first added chain gets index 0");
}

// ── Weapon Animation Tests ──────────────────────────────────────────────────

void test_sway_layer_identity_on_zero_input() {
    ae::animation::WeaponAnimState state;
    ae::animation::WeaponAnimConfig config;

    ae::skeleton::Mat4 offset = ae::skeleton::Mat4::identity();
    ae::animation::evaluate_sway_layer(state, config, 0.016F, 0.0F, 0.0F, 0.0F, offset);

    // With zero look delta, sway should remain at identity (or nearly so)
    bool near_identity = true;
    for (int i = 0; i < 16; ++i) {
        float expected = (i % 5 == 0) ? 1.0F : 0.0F; // diagonal = 1, else 0
        if (!nearly_equal(offset.m[i], expected, 1.0e-2F)) {
            near_identity = false;
            break;
        }
    }
    check(near_identity, "sway layer at identity with zero look input");
}

void test_bob_layer_no_movement() {
    ae::animation::WeaponAnimState state;
    ae::animation::WeaponAnimConfig config;

    ae::skeleton::Mat4 offset;
    ae::animation::evaluate_bob_layer(state, config, 0.016F, 0.0F, false, offset);

    // When not moving, bob should remain near identity
    bool near_identity = true;
    for (int i = 0; i < 16; ++i) {
        float expected = (i % 5 == 0) ? 1.0F : 0.0F;
        if (!nearly_equal(offset.m[i], expected, 1.0e-2F)) {
            near_identity = false;
            break;
        }
    }
    check(near_identity, "bob layer near identity when player is not moving");
}

void test_recoil_kick_layer_identity_no_fire() {
    ae::animation::WeaponAnimState state;
    ae::animation::WeaponAnimConfig config;

    ae::skeleton::Mat4 offset;
    ae::animation::evaluate_recoil_kick_layer(state, config, 0.016F, offset);

    // Without firing, recoil kick should be identity
    bool near_identity = true;
    for (int i = 0; i < 16; ++i) {
        float expected = (i % 5 == 0) ? 1.0F : 0.0F;
        if (!nearly_equal(offset.m[i], expected, 1.0e-2F)) {
            near_identity = false;
            break;
        }
    }
    check(near_identity, "recoil kick layer at identity when no fire");
}

void test_fire_weapon_kick_triggers_recoil() {
    ae::animation::WeaponAnimState state;
    ae::animation::WeaponAnimConfig config;

    ae::animation::fire_weapon_kick(state);
    check(state.fire_anim_time > 0.0F, "fire_weapon_kick sets fire_anim_time > 0");

    // After kick, the recoil layer should produce a non-identity transform
    ae::skeleton::Mat4 offset;
    ae::animation::evaluate_recoil_kick_layer(state, config, 0.016F, offset);

    // The kick produces a Y translation offset of -kick in m[13].
    // With a tighter tolerance, we can detect this small offset.
    bool has_kick_offset = !nearly_equal(offset.m[13], 0.0F, 1.0e-4F);
    check(has_kick_offset, "recoil kick layer produces non-zero Y translation after fire");

    // After enough time, the kick should decay
    for (int i = 0; i < 120; ++i) {
        ae::animation::evaluate_recoil_kick_layer(state, config, 0.016F, offset);
    }
    check(state.fire_anim_time < 0.001F, "recoil kick decays to zero");
}

void test_weapon_anim_state_reset() {
    ae::animation::WeaponAnimState state;
    state.sway_phase = 1.5F;
    state.bob_phase = 2.0F;
    state.ads_blend = 0.5F;
    state.fire_anim_time = 1.0F;
    state.is_firing = true;
    state.is_reloading = true;

    // Reset (construct a fresh default state)
    state = ae::animation::WeaponAnimState{};

    check(nearly_equal(state.sway_phase, 0.0F), "sway_phase resets to 0");
    check(nearly_equal(state.bob_phase, 0.0F), "bob_phase resets to 0");
    check(nearly_equal(state.ads_blend, 0.0F), "ads_blend resets to 0");
    check(nearly_equal(state.fire_anim_time, 0.0F), "fire_anim_time resets to 0");
    check(!state.is_firing, "is_firing resets to false");
    check(!state.is_reloading, "is_reloading resets to false");
}

// ── Blend Space Tests ───────────────────────────────────────────────────────

void test_blend_1d_get_pair() {
    ae::animation::BlendSpace1D blend;
    blend.samples.push_back({"clip_a", 0.0F});
    blend.samples.push_back({"clip_b", 1.0F});
    blend.current_parameter = 0.5F;

    std::size_t idx_a = 99, idx_b = 99;
    float t = 0.0F;
    bool ok = blend.get_blend_pair(idx_a, idx_b, t);

    check(ok, "1D blend returns valid pair");
    check(idx_a == 0 && idx_b == 1, "1D blend selects correct samples");
    check(nearly_equal(t, 0.5F), "1D blend factor is 0.5 at midpoint");
}

void test_blend_1d_out_of_range_clamps() {
    ae::animation::BlendSpace1D blend;
    blend.samples.push_back({"clip_a", 0.0F});
    blend.samples.push_back({"clip_b", 10.0F});
    blend.current_parameter = 20.0F;  // beyond last sample

    std::size_t idx_a = 99, idx_b = 99;
    float t = 0.0F;
    bool ok = blend.get_blend_pair(idx_a, idx_b, t);

    check(ok, "1D blend handles out-of-range parameter");
}

void test_blend_1d_single_sample() {
    ae::animation::BlendSpace1D blend;
    blend.samples.push_back({"clip_a", 0.0F});
    blend.current_parameter = 0.0F;

    std::size_t idx_a = 99, idx_b = 99;
    float t = 0.0F;
    bool ok = blend.get_blend_pair(idx_a, idx_b, t);

    check(!ok, "1D blend with single sample returns false");
}

} // anonymous namespace

int main() {
    std::cout << "animation_state_machine_tests:\n";

    // State Machine
    test_sm_initial_state();
    test_sm_trigger_transition();
    test_sm_transition_completes();
    test_sm_active_clips_during_transition();
    test_sm_single_clip_after_transition();
    test_sm_unknown_trigger_does_nothing();
    test_sm_set_blend_param();

    // Animation Graph
    test_graph_evaluate_single_clip();
    test_graph_evaluate_with_state_machine();

    // IK Solver
    test_ik_two_bone_reachable();
    test_ik_two_bone_overextended();
    test_ik_two_bone_degenerate();
    test_ik_two_bone_disabled();
    test_ik_add_chain();

    // Weapon Animation
    test_sway_layer_identity_on_zero_input();
    test_bob_layer_no_movement();
    test_recoil_kick_layer_identity_no_fire();
    test_fire_weapon_kick_triggers_recoil();
    test_weapon_anim_state_reset();

    // Blend Spaces
    test_blend_1d_get_pair();
    test_blend_1d_out_of_range_clamps();
    test_blend_1d_single_sample();

    if (failures == 0) {
        std::cout << "animation_state_machine_tests: all ok\n";
        return 0;
    }
    std::cerr << "animation_state_machine_tests: " << failures << " test(s) failed\n";
    return 1;
}

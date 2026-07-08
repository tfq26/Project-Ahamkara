#include "ahamkara/client/playtest_harness.h"

#include "ae/core/log.h"
#include "ae/core/math.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {
namespace {

float distance_between(const ahamkara::game::Vec3& a, const ahamkara::game::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void append_step_command(
    const PlaytestScenarioStep& step,
    ahamkara::game::PlayerInputCommand& command,
    bool pulse_consumed) {
    command.move_axis = step.move_axis;
    command.look_delta = step.look_delta;
    command.crouch_held = step.crouch_held;
    command.sprint_held = step.sprint_held;
    command.slide_pressed = step.slide_pressed && !pulse_consumed;
    command.fire_held = step.fire_held;
    command.reload_pressed = step.reload_pressed && !pulse_consumed;
    command.ability_pressed = step.ability_pressed && !pulse_consumed;
    command.interact_pressed = step.interact_pressed && !pulse_consumed;
    command.jump_pressed = step.jump_pressed && !pulse_consumed;
    command.weapon_slot = step.weapon_slot;
}

}  // namespace

ScenarioInputProvider::ScenarioInputProvider(PlaytestScenario scenario)
    : scenario_(std::move(scenario)) {
    if (scenario_.steps.empty()) {
        finished_ = true;
    }
}

ahamkara::game::PlayerInputCommand ScenarioInputProvider::gather_input(float delta_seconds) {
    ahamkara::game::PlayerInputCommand command {};
    if (finished_ || step_index_ >= scenario_.steps.size()) {
        finished_ = true;
        return command;
    }

    const auto& step = scenario_.steps[step_index_];
    append_step_command(step, command, pulse_consumed_);
    current_step_label_ = step.label;
    pulse_consumed_ = true;

    step_elapsed_seconds_ += delta_seconds;
    if (step.duration_seconds <= 0.0F || step_elapsed_seconds_ >= step.duration_seconds) {
        advance_step();
    }

    return command;
}

bool ScenarioInputProvider::finished() const {
    return finished_;
}

void ScenarioInputProvider::advance_step() {
    step_elapsed_seconds_ = 0.0F;
    pulse_consumed_ = false;
    ++step_index_;
    if (step_index_ >= scenario_.steps.size()) {
        finished_ = true;
        current_step_label_.clear();
    }
}

PlaytestScenario make_default_autoplay_scenario(const std::string& level_path) {
    PlaytestScenario scenario;
    scenario.name = "flashback-autoplay";
    scenario.level_path = level_path;
    scenario.add_spawn_training_target = true;
    scenario.spawn_training_target_offset = {0.5F, 0.0F, 0.0F};
    scenario.max_duration_seconds = 10.0F;
    scenario.settle_seconds_after_completion = 0.35F;
    scenario.require_movement = true;
    scenario.minimum_movement_distance = 0.2F;
    scenario.require_interaction = true;
    scenario.minimum_interactions = 1;
    scenario.require_fire = true;
    scenario.require_reload = true;
    scenario.require_ability = true;

    auto make_step = [](float duration, const char* label) {
        PlaytestScenarioStep step;
        step.duration_seconds = duration;
        step.label = label;
        return step;
    };

    {
        PlaytestScenarioStep settle = make_step(0.25F, "settle");
        scenario.steps.push_back(settle);
    }
    {
        PlaytestScenarioStep interact = make_step(0.10F, "interact");
        interact.interact_pressed = true;
        scenario.steps.push_back(interact);
    }
    {
        PlaytestScenarioStep approach = make_step(0.75F, "approach-target");
        approach.move_axis = {1.0F, 0.0F};
        approach.sprint_held = true;
        scenario.steps.push_back(approach);
    }
    {
        PlaytestScenarioStep fire = make_step(0.75F, "fire");
        fire.move_axis = {0.0F, 1.0F};
        fire.sprint_held = true;
        fire.fire_held = true;
        scenario.steps.push_back(fire);
    }
    {
        PlaytestScenarioStep reload = make_step(0.10F, "reload");
        reload.reload_pressed = true;
        scenario.steps.push_back(reload);
    }
    {
        PlaytestScenarioStep ability = make_step(0.10F, "ability");
        ability.ability_pressed = true;
        scenario.steps.push_back(ability);
    }
    {
        PlaytestScenarioStep jump = make_step(0.40F, "jump");
        jump.jump_pressed = true;
        scenario.steps.push_back(jump);
    }

    return scenario;
}

PlaytestRunResult run_playtest_scenario(const PlaytestScenario& scenario) {
    PlaytestRunResult result;
    auto provider = std::make_unique<ScenarioInputProvider>(scenario);
    ScenarioInputProvider* provider_view = provider.get();
    LocalPlaySimulation simulation(std::move(provider));

    if (!scenario.level_path.empty()) {
        if (!simulation.load_level(scenario.level_path)) {
            result.summary = "failed to load level: " + scenario.level_path;
            ae::log_warning(result.summary);
            return result;
        }
    }

    std::vector<ahamkara::game::InteractionTargetDefinition> targets = scenario.interaction_targets;
    if (scenario.add_spawn_training_target) {
        const auto spawn = simulation.get_player_state().position;
        targets.push_back(ahamkara::game::InteractionTargetDefinition {
            9001,
            {
                spawn.x + scenario.spawn_training_target_offset.x,
                spawn.y + scenario.spawn_training_target_offset.y,
                spawn.z + scenario.spawn_training_target_offset.z
            },
            1.5F,
            true,
            "autoplay_terminal"
        });
    }
    if (!targets.empty()) {
        simulation.set_interaction_targets(targets.data(), targets.size());
    }

    const auto start_pos = simulation.get_player_state().position;
    const int initial_ammo = simulation.get_ammo_current();
    const double step_seconds = simulation.get_fixed_step_seconds();
    const int max_ticks = static_cast<int>(std::ceil(scenario.max_duration_seconds / step_seconds));
    const int settle_ticks = static_cast<int>(std::ceil(scenario.settle_seconds_after_completion / step_seconds));

    int post_complete_ticks = 0;
    for (int tick = 0; tick < max_ticks; ++tick) {
        simulation.tick(static_cast<float>(step_seconds));
        ++result.ticks;
        result.simulated_seconds += static_cast<float>(step_seconds);

        if (provider_view->finished()) {
            ++post_complete_ticks;
            if (post_complete_ticks >= std::max(1, settle_ticks)) {
                break;
            }
        }
    }

    result.movement_distance = distance_between(start_pos, simulation.get_player_state().position);
    result.interaction_attempts = simulation.get_interaction_attempt_count();
    result.interaction_successes = simulation.get_interaction_success_count();
    const int reload_requests = simulation.get_reload_request_count();
    const int ability_uses = simulation.get_ability_use_count();
    result.ammo_spent = std::max(0, initial_ammo - simulation.get_ammo_current());

    bool passed = true;
    if (scenario.require_movement) {
        passed = passed && result.movement_distance >= scenario.minimum_movement_distance;
    }
    if (scenario.require_interaction) {
        passed = passed && result.interaction_successes >= scenario.minimum_interactions;
    }
    if (scenario.require_fire) {
        passed = passed && result.ammo_spent > 0;
    }
    if (scenario.require_reload) {
        passed = passed && reload_requests > 0;
    }
    if (scenario.require_ability) {
        passed = passed && ability_uses > 0;
    }

    result.passed = passed && provider_view->finished();
    result.timed_out = !provider_view->finished();

    std::ostringstream summary;
    summary << "scenario='" << scenario.name << "' "
            << "passed=" << (result.passed ? "true" : "false") << ' '
            << "ticks=" << result.ticks << ' '
            << "seconds=" << result.simulated_seconds << ' '
            << "movement=" << result.movement_distance << ' '
            << "interactions=" << result.interaction_successes << '/' << result.interaction_attempts << ' '
            << "ammo_spent=" << result.ammo_spent << ' '
            << "reloads=" << reload_requests << ' '
            << "abilities=" << ability_uses;
    if (result.timed_out) {
        summary << " timed_out=true";
    }
    result.summary = summary.str();

    if (result.passed) {
        ae::log_info(result.summary);
    } else {
        ae::log_warning(result.summary);
    }

    return result;
}

}  // namespace ahamkara::client

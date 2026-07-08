#pragma once

#include "ahamkara/client/local_play.h"
#include "ahamkara/game/worlds/world_definition.h"

#include <memory>
#include <string>
#include <vector>

namespace ahamkara::client {

struct PlaytestScenarioStep {
    float duration_seconds {0.0F};
    ahamkara::game::Vec2 move_axis {};
    ahamkara::game::Vec2 look_delta {};
    bool jump_pressed {false};
    bool crouch_held {false};
    bool sprint_held {false};
    bool slide_pressed {false};
    bool fire_held {false};
    bool reload_pressed {false};
    bool ability_pressed {false};
    bool interact_pressed {false};
    ae::u8 weapon_slot {0};
    std::string label {};
};

struct PlaytestScenario {
    std::string name {"autoplay"};
    std::string level_path {};
    std::vector<PlaytestScenarioStep> steps {};
    std::vector<ahamkara::game::InteractionTargetDefinition> interaction_targets {};
    bool add_spawn_training_target {true};
    ahamkara::game::Vec3 spawn_training_target_offset {1.25F, 0.0F, 0.0F};
    float max_duration_seconds {12.0F};
    float settle_seconds_after_completion {0.25F};
    bool require_movement {true};
    float minimum_movement_distance {1.0F};
    bool require_interaction {true};
    int minimum_interactions {1};
    bool require_fire {true};
    bool require_reload {false};
    bool require_ability {false};
};

struct PlaytestRunResult {
    bool passed {false};
    bool timed_out {false};
    float simulated_seconds {0.0F};
    ae::u32 ticks {0};
    float movement_distance {0.0F};
    int interaction_attempts {0};
    int interaction_successes {0};
    int ammo_spent {0};
    std::string summary {};
};

class ScenarioInputProvider final : public IInputProvider {
public:
    explicit ScenarioInputProvider(PlaytestScenario scenario);

    [[nodiscard]] ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) override;
    [[nodiscard]] bool finished() const override;
    [[nodiscard]] const PlaytestScenario& scenario() const { return scenario_; }
    [[nodiscard]] std::size_t current_step_index() const { return step_index_; }
    [[nodiscard]] const std::string& current_step_label() const { return current_step_label_; }

private:
    void advance_step();

    PlaytestScenario scenario_;
    std::size_t step_index_ {0};
    float step_elapsed_seconds_ {0.0F};
    bool pulse_consumed_ {false};
    bool finished_ {false};
    std::string current_step_label_ {};
};

[[nodiscard]] PlaytestScenario make_default_autoplay_scenario(const std::string& level_path = {});

[[nodiscard]] PlaytestRunResult run_playtest_scenario(const PlaytestScenario& scenario);

}  // namespace ahamkara::client

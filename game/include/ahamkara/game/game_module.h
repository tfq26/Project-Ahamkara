#pragma once

namespace ahamkara::game {

const char* game_name();

/** Register all game config variables for hot-reload support. */
void register_game_config();

// Movement tuning, sourced from the hot-reloadable `game.*` config vars so the
// runtime movement model can be tuned without recompiling. Defaults match the
// engine's prior movement constants (this is behavior-preserving).
[[nodiscard]] float cfg_walk_speed();
[[nodiscard]] float cfg_sprint_speed();
[[nodiscard]] float cfg_jump_speed();
[[nodiscard]] float cfg_gravity();

}  // namespace ahamkara::game

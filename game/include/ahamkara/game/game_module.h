#pragma once

namespace ahamkara::game {

const char* game_name();

/** Register all game config variables for hot-reload support. */
void register_game_config();

}  // namespace ahamkara::game


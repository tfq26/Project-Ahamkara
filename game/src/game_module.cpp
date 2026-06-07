#include "ahamkara/game/game_module.h"

#include "ae/core/config.h"
#include "ae/core/log.h"

namespace ahamkara::game {

const char* game_name() {
    return "Ahamkara";
}

// ── Gameplay config variables (hot-reloadable) ──

static ae::ConfigVar<float> g_player_speed("game.player_speed", 5.5F);
static ae::ConfigVar<float> g_player_sprint_mult("game.player_sprint_mult", 1.6F);
static ae::ConfigVar<float> g_player_jump_velocity("game.player_jump_velocity", 7.5F);
static ae::ConfigVar<float> g_player_gravity("game.player_gravity", 15.0F);
static ae::ConfigVar<float> g_projectile_speed("game.projectile_speed", 120.0F);
static ae::ConfigVar<float> g_projectile_damage("game.projectile_damage", 25.0F);
static ae::ConfigVar<int>   g_max_projectiles("game.max_projectiles", 64);
static ae::ConfigVar<float> g_dummy_health("game.dummy_health", 100.0F);
static ae::ConfigVar<float> g_dummy_respawn_time("game.dummy_respawn_time", 3.0F);
static ae::ConfigVar<bool>  g_debug_physics("debug.show_physics", false);
static ae::ConfigVar<bool>  g_debug_hitboxes("debug.show_hitboxes", false);

void register_game_config() {
    // Set up change callbacks for config vars that need runtime notification
    g_player_speed.on_change([](float old_val, float new_val) {
        ae::log_info_cat("Config", "player_speed: " + std::to_string(old_val) +
                         " -> " + std::to_string(new_val));
    });
    g_player_sprint_mult.on_change([](float old_val, float new_val) {
        ae::log_info_cat("Config", "player_sprint_mult: " + std::to_string(old_val) +
                         " -> " + std::to_string(new_val));
    });

    ae::log_info_cat("Config", "Game config variables registered.");
}

}  // namespace ahamkara::game


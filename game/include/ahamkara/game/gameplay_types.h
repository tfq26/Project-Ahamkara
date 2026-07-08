#pragma once

#include "ahamkara/game/net_types.h"
#include "ahamkara/game/components.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace ahamkara::game {

// --- Team / Friendly Fire --------------------------------------------------

enum class Team : int {
    None = 0,
    Spectator = 1,
    Red = 2,
    Blue = 3,
    FFA = 4
};

struct TeamColor {
    float r {1.0F};
    float g {1.0F};
    float b {1.0F};
    float a {1.0F};
};

inline TeamColor team_color(Team team) {
    switch (team) {
        case Team::Red: return {1.0F, 0.0F, 0.0F, 1.0F};
        case Team::Blue: return {0.0F, 0.0F, 1.0F, 1.0F};
        default: return {1.0F, 1.0F, 1.0F, 1.0F};
    }
}

inline bool can_damage(Team attacker, Team target, Team match_team_mode) {
    if (attacker == Team::None || attacker == Team::Spectator) return false;
    if (target == Team::None || target == Team::Spectator) return false;
    if (match_team_mode == Team::FFA) return true;
    return attacker != target;
}

// --- Game Mode -------------------------------------------------------------

enum class GameModeType {
    Deathmatch,
    TeamDeathmatch,
    CaptureTheFlag,
    KingOfTheHill,
    SearchAndDestroy
};

struct GameModeRules {
    GameModeType type {GameModeType::Deathmatch};
    int score_limit {50};
    int max_players {12};
    float respawn_time {3.0F};
    bool auto_start {true};
    bool friendly_fire {false};
    float time_limit_minutes {0.0F};
    int round_count {1};
};

// --- Spawn System -----------------------------------------------------------

struct SpawnPoint {
    Vec3 position {};
    float yaw {0.0F};
    Team team {Team::None};
    int priority {0};
    bool enabled {true};
};

class SpawnSelector {
public:
    SpawnSelector() {
        std::fill(std::begin(m_last_used_tick), std::end(m_last_used_tick), 0U);
    }

    bool select(const std::vector<SpawnPoint>& spawns, Team team, ae::u32 current_tick, Vec3& out_position, float& out_yaw);
    void mark_used(int index, ae::u32 tick);

private:
    static constexpr int kMaxLastUsed = 256;
    ae::u32 m_last_used_tick[kMaxLastUsed];
};

// --- Damage Model / Health & Armor ------------------------------------------

struct DamageEvent {
    float health_damage {0.0F};
    float armor_damage {0.0F};
    bool was_lethal {false};
    bool is_headshot {false};
};

struct ArmorConfig {
    float absorption_fraction {0.66F};
    float durability_ratio {2.0F};
    bool protects_head {true};
    float helmet_multiplier {0.5F};
};

inline bool apply_damage(HealthComponent& hc, float raw_damage, bool headshot, const ArmorConfig& armor, DamageEvent& event) {
    event.is_headshot = headshot;
    
    bool has_helmet = headshot && armor.protects_head && (hc.shield > 0.0F);
    float actual_raw_damage = raw_damage;
    
    float damage_to_absorb = 0.0F;
    if (!headshot || has_helmet) {
        if (has_helmet) {
            actual_raw_damage *= armor.helmet_multiplier;
        }
        damage_to_absorb = actual_raw_damage * armor.absorption_fraction;
    }

    float max_absorbed = hc.shield / armor.durability_ratio;
    float absorbed = std::min(damage_to_absorb, max_absorbed);
    
    event.armor_damage = absorbed;
    event.health_damage = std::min(actual_raw_damage - absorbed, hc.current);

    hc.shield -= absorbed * armor.durability_ratio;
    if (hc.shield < 0.0F) hc.shield = 0.0F;

    hc.current -= event.health_damage;
    if (hc.current <= 0.0F) {
        hc.current = 0.0F;
        event.was_lethal = true;
        return false; // dead
    }
    event.was_lethal = false;
    return true; // survived
}

// --- Status Effects ---------------------------------------------------------

enum class StatusEffectType : int {
    None = 0,
    Burning = 1,
    Poison = 2,
    Slowed = 3,
    Stunned = 4,
    Frozen = 5,
    Blinded = 6,
    Silenced = 7,
    Cloaked = 8,
    Count = 9
};

struct StatusEffectInstance {
    StatusEffectType type {StatusEffectType::None};
    float duration_remaining {0.0F};
    float tick_interval {1.0F};
    float magnitude {1.0F};
};

struct KillFeedEntry {
    ae::u32 attacker_id {0};
    ae::u32 victim_id {0};
    bool is_headshot {false};
    std::string weapon_name {};
};

// --- Weapon Framework -------------------------------------------------------

enum class WeaponSlot : int {
    Primary = 0,
    Secondary = 1,
    Melee = 2,
    Count = 3
};

enum class FireMode : int {
    Hitscan = 0,
    Projectile = 1,
    Burst = 2,
    Automatic = 3
};

struct RecoilEntry {
    float pitch {0.0F};
    float yaw {0.0F};
};

struct WeaponDefinition {
    int magazine_size {30};
    float base_damage {25.0F};
    float headshot_multiplier {2.0F};
    FireMode fire_mode {FireMode::Hitscan};
    WeaponSlot slot {WeaponSlot::Primary};
    float rpm {400.0F};             ///< Rounds per minute — used to derive fire cooldown.
    float reload_time_s {2.0F};     ///< Reload duration in seconds.
    int reserve_ammo_max {150};     ///< Max reserve ammo (0 = use 3x magazine as default).
    std::vector<RecoilEntry> recoil_pattern {};

    /// Seconds between shots derived from RPM.
    [[nodiscard]] float fire_interval() const { return 60.0F / std::max(rpm, 1.0F); }
};

struct WeaponState {
    int ammo_in_magazine {30};
    int reserve_ammo {90};
    int magazine_capacity {30};
    bool is_reloading {false};
    bool is_equipping {false};
    bool is_charging {false};
    float fire_cooldown {0.0F};
    int definition_index {0};

    bool can_fire() const {
        return ammo_in_magazine > 0 && !is_reloading && !is_equipping && !is_charging && fire_cooldown <= 0.0F;
    }

    bool can_reload() const {
        return ammo_in_magazine < magazine_capacity && reserve_ammo > 0 && !is_reloading && !is_equipping;
    }
};

struct Loadout {
    int active_slot {0};
    int weapons[static_cast<int>(WeaponSlot::Count)] {0};
};

struct PlayerLoadoutSelection {
    int primary_weapon_index {0};
    int secondary_weapon_index {0};
    int melee_weapon_index {0};
};

// --- Match State Machine ----------------------------------------------------

enum class MatchPhase : int {
    Lobby = 0,
    Warmup = 1,
    Countdown = 2,
    InProgress = 3,
    Overtime = 4,
    RoundEnd = 5,
    MatchEnd = 6,
    PostMatch = 7
};

struct MatchState {
    MatchPhase phase {MatchPhase::Lobby};
    float phase_timer {0.0F};
    float match_time {0.0F};
    ae::u32 team_score_red {0};
    ae::u32 team_score_blue {0};
    ae::u16 individual_score[12] {0};
    ae::u32 match_winner_id {0};
    int current_round {0};

    bool is_playing() const {
        return phase == MatchPhase::InProgress || phase == MatchPhase::Overtime;
    }

    bool tick(float delta_seconds, const GameModeRules& rules);
    void set_phase(MatchPhase new_phase, float duration);
    void add_score(Team team, ae::u32 player_id, ae::u16 score);
};

// --- Replay Frame -----------------------------------------------------------

struct ReplayFrameHeader {
    ae::u32 tick {0};
    float delta_seconds {0.016F};
    int input_command_count {0};
    int event_count {0};
};

} // namespace ahamkara::game

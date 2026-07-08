#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/components.h"
#include "ahamkara/game/player.h"
#include "ahamkara/game/weapon_runtime.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

using namespace ahamkara::game;

// ===================================================================
//  Team / Friendly Fire  (95)
// ===================================================================

void test_team_enums() {
    assert(static_cast<int>(Team::None) == 0);
    assert(static_cast<int>(Team::Red) == 2);
    assert(static_cast<int>(Team::Blue) == 3);
    std::cout << "test_team_enums passed.\n";
}

void test_can_damage_team_mode() {
    // Team Deathmatch: Red vs Blue
    assert(can_damage(Team::Red, Team::Blue, Team::Red));   // enemies
    assert(!can_damage(Team::Red, Team::Red, Team::Red));   // same team
    assert(!can_damage(Team::Blue, Team::Blue, Team::Red)); // same team

    // FFA: everyone damages everyone
    assert(can_damage(Team::Red, Team::Red, Team::FFA));
    assert(can_damage(Team::Blue, Team::Blue, Team::FFA));

    // Spectators/None cannot damage or be damaged
    assert(!can_damage(Team::Spectator, Team::Red, Team::Red));
    assert(!can_damage(Team::Red, Team::Spectator, Team::Red));
    assert(!can_damage(Team::None, Team::Red, Team::Red));
    assert(!can_damage(Team::Red, Team::None, Team::Red));

    std::cout << "test_can_damage_team_mode passed.\n";
}

// ===================================================================
//  Game Mode  (95)
// ===================================================================

void test_game_mode_rules_defaults() {
    GameModeRules rules{};
    assert(rules.type == GameModeType::Deathmatch);
    assert(rules.score_limit == 50);
    assert(rules.max_players == 12);
    assert(rules.respawn_time == 3.0F);
    assert(rules.auto_start);
    assert(!rules.friendly_fire);
    std::cout << "test_game_mode_rules_defaults passed.\n";
}

// ===================================================================
//  Spawn System  (96)
// ===================================================================

void test_spawn_selector_basic() {
    std::vector<SpawnPoint> spawns = {
        { {0.0F, 0.0F, 0.0F},  0.0F, Team::None, 0, true },
        { {10.0F, 0.0F, 0.0F}, 90.0F, Team::Red, 1, true },
        { {-10.0F, 0.0F, 0.0F}, 270.0F, Team::Blue, 1, true },
    };

    SpawnSelector sel{};
    Vec3 pos{};
    float yaw{};

    // Red player should prefer priority-1 Red spawn
    assert(sel.select(spawns, Team::Red, 0, pos, yaw));
    assert(pos.x == 10.0F);
    assert(yaw == 90.0F);

    // Blue player similarly
    assert(sel.select(spawns, Team::Blue, 0, pos, yaw));
    assert(pos.x == -10.0F);
    assert(yaw == 270.0F);

    // Team::None gets the neutral spawn (only available choice)
    assert(sel.select(spawns, Team::None, 0, pos, yaw));
    assert(pos.x == 0.0F);

    std::cout << "test_spawn_selector_basic passed.\n";
}

void test_spawn_selector_avoids_recent() {
    std::vector<SpawnPoint> spawns = {
        { {0.0F, 0.0F, 0.0F}, 0.0F, Team::None, 1, true },
        { {5.0F, 0.0F, 0.0F}, 0.0F, Team::None, 1, true },
        { {10.0F, 0.0F, 0.0F}, 0.0F, Team::None, 1, true },
    };

    SpawnSelector sel{};
    Vec3 pos{};
    float yaw{};

    // First spawn: any valid (all same prio, none marked used)
    assert(sel.select(spawns, Team::None, 100, pos, yaw));
    // Mark that index as used
    sel.mark_used(0, 100);

    // Next spawn should avoid index 0 (it was recently used)
    assert(sel.select(spawns, Team::None, 200, pos, yaw));
    // Should prefer not-index-0 since it was just used
    assert(pos.x != 0.0F);  // index 0 is at x=0.0

    std::cout << "test_spawn_selector_avoids_recent passed.\n";
}

void test_spawn_selector_disabled() {
    std::vector<SpawnPoint> spawns = {
        { {10.0F, 0.0F, 0.0F}, 0.0F, Team::None, 1, false },  // disabled
    };

    SpawnSelector sel{};
    Vec3 pos{};
    float yaw{};

    assert(!sel.select(spawns, Team::None, 0, pos, yaw));

    std::cout << "test_spawn_selector_disabled passed.\n";
}

void test_spawn_selector_empty() {
    SpawnSelector sel{};
    Vec3 pos{};
    float yaw{};
    assert(!sel.select({}, Team::None, 0, pos, yaw));
    std::cout << "test_spawn_selector_empty passed.\n";
}

// ===================================================================
//  Damage Model  (93, 94)
// ===================================================================

void test_damage_basic_no_armor() {
    HealthComponent hc{};
    hc.current = 100.0F;
    hc.shield = 0.0F;

    DamageEvent event{};
    ArmorConfig armor{};

    bool survived = apply_damage(hc, 25.0F, false, armor, event);

    assert(survived);
    assert(hc.current == 75.0F);
    assert(event.health_damage == 25.0F);
    assert(event.armor_damage == 0.0F);
    assert(!event.was_lethal);

    std::cout << "test_damage_basic_no_armor passed.\n";
}

void test_damage_lethal() {
    HealthComponent hc{};
    hc.current = 30.0F;
    hc.shield = 0.0F;

    DamageEvent event{};
    ArmorConfig armor{};

    bool survived = apply_damage(hc, 50.0F, false, armor, event);

    assert(!survived);
    assert(hc.current == 0.0F);
    assert(event.health_damage == 30.0F);  // capped to remaining health
    assert(event.was_lethal);

    std::cout << "test_damage_lethal passed.\n";
}

void test_damage_with_armor() {
    HealthComponent hc{};
    hc.current = 100.0F;
    hc.shield = 50.0F;

    DamageEvent event{};
    ArmorConfig armor{};

    // 30 raw damage → 66% absorbed by armor = 19.8
    // Armor cost: 19.8 * 2.0 = 39.6 from shield
    // Health takes: 30 - 19.8 = 10.2
    bool survived = apply_damage(hc, 30.0F, false, armor, event);

    assert(survived);
    assert(event.health_damage > 10.0F && event.health_damage < 11.0F);
    assert(event.armor_damage > 19.0F && event.armor_damage < 20.0F);
    assert(hc.shield < 15.0F);  // 50 - 39.6 ≈ 10.4
    assert(hc.shield > 5.0F);

    std::cout << "test_damage_with_armor passed.\n";
}

void test_damage_armor_depleted() {
    HealthComponent hc{};
    hc.current = 100.0F;
    hc.shield = 5.0F;  // very low armor

    DamageEvent event{};
    ArmorConfig armor{};

    // With only 5 armor: absorbs at most 5/2.0 = 2.5 damage
    // So 66% * 50 = 33 would be absorbable, but only 2.5 available
    bool survived = apply_damage(hc, 50.0F, false, armor, event);

    assert(survived);
    assert(hc.shield == 0.0F);                // armor gone
    assert(event.armor_damage <= 3.0F);       // limited by armor pool
    assert(event.health_damage > 47.0F);

    std::cout << "test_damage_armor_depleted passed.\n";
}

void test_damage_headshot() {
    HealthComponent hc{};
    hc.current = 100.0F;
    hc.shield = 0.0F;

    DamageEvent event{};
    ArmorConfig armor{};

    // Headshot damage with 2x multiplier applied by caller
    float headshot_damage = 25.0F * 2.0F;
    bool survived = apply_damage(hc, headshot_damage, true, armor, event);

    assert(survived);
    assert(event.health_damage == 50.0F);
    assert(event.is_headshot);

    std::cout << "test_damage_headshot passed.\n";
}

void test_armor_config_defaults() {
    ArmorConfig armor{};
    assert(armor.absorption_fraction == 0.66F);
    assert(armor.durability_ratio == 2.0F);
    assert(armor.protects_head);
    assert(armor.helmet_multiplier == 0.5F);
    std::cout << "test_armor_config_defaults passed.\n";
}

// ===================================================================
//  Match State Machine  (98)
// ===================================================================

void test_match_state_initial() {
    MatchState ms{};
    assert(ms.phase == MatchPhase::Lobby);
    assert(!ms.is_playing());
    assert(ms.match_time == 0.0F);
    assert(ms.team_score_red == 0);
    assert(ms.team_score_blue == 0);
    std::cout << "test_match_state_initial passed.\n";
}

void test_match_state_progression() {
    MatchState ms{};
    GameModeRules rules{};

    // Lobby → Warmup (auto_start = true)
    assert(ms.tick(0.0F, rules));
    assert(ms.phase == MatchPhase::Warmup);
    assert(ms.phase_timer == 15.0F);

    // Warmup → Countdown after 15s
    assert(ms.tick(15.0F, rules));
    assert(ms.phase == MatchPhase::Countdown);
    assert(ms.phase_timer == 3.0F);

    // Countdown → InProgress after 3s
    assert(ms.tick(3.0F, rules));
    assert(ms.phase == MatchPhase::InProgress);
    assert(ms.is_playing());

    std::cout << "test_match_state_progression passed.\n";
}

void test_match_state_score_limit() {
    MatchState ms{};
    GameModeRules rules{};
    rules.score_limit = 10;

    // Progress to InProgress
    ms.tick(0.0F, rules);    // → Warmup
    ms.tick(15.0F, rules);   // → Countdown
    ms.tick(3.0F, rules);    // → InProgress

    // Score up to limit
    ms.add_score(Team::Red, 0, 5);
    ms.add_score(Team::Red, 0, 6);  // 11 total (past 10)

    bool changed = ms.tick(0.016F, rules);
    assert(changed);
    assert(ms.phase == MatchPhase::MatchEnd);

    std::cout << "test_match_state_score_limit passed.\n";
}

void test_match_state_time_limit() {
    MatchState ms{};
    GameModeRules rules{};
    rules.time_limit_minutes = 1.0F;

    // Progress to InProgress
    ms.tick(0.0F, rules);
    ms.tick(15.0F, rules);
    ms.tick(3.0F, rules);

    // Simulate 65 seconds of gameplay (past 1 min limit)
    // But first: make sure scores are different to avoid overtime
    ms.add_score(Team::Red, 0, 1);

    bool changed = ms.tick(65.0F, rules);
    assert(changed);
    assert(ms.phase == MatchPhase::MatchEnd);

    std::cout << "test_match_state_time_limit passed.\n";
}

void test_match_state_overtime() {
    MatchState ms{};
    GameModeRules rules{};
    rules.score_limit = 10;

    // Progress to InProgress
    ms.tick(0.0F, rules);
    ms.tick(15.0F, rules);
    ms.tick(3.0F, rules);

    // Both teams hit limit simultaneously → overtime
    ms.add_score(Team::Red, 0, 10);
    ms.add_score(Team::Blue, 1, 10);

    bool changed = ms.tick(0.016F, rules);
    assert(changed);
    assert(ms.phase == MatchPhase::Overtime);

    // In overtime, a score difference ends it
    ms.add_score(Team::Red, 0, 1);
    changed = ms.tick(0.016F, rules);
    assert(changed);
    assert(ms.phase == MatchPhase::MatchEnd);

    std::cout << "test_match_state_overtime passed.\n";
}

void test_match_state_round_transition() {
    MatchState ms{};
    GameModeRules rules{};
    rules.round_count = 2;

    // Progress to InProgress
    ms.tick(0.0F, rules);
    ms.tick(15.0F, rules);
    ms.tick(3.0F, rules);

    // End round manually
    ms.set_phase(MatchPhase::RoundEnd, 5.0F);
    assert(ms.phase == MatchPhase::RoundEnd);

    // After 5s, should go to Countdown for round 2
    assert(ms.tick(5.0F, rules));
    assert(ms.phase == MatchPhase::Countdown);
    assert(ms.current_round == 1);

    // Progress to InProgress
    assert(ms.tick(3.0F, rules));
    assert(ms.phase == MatchPhase::InProgress);

    // End round 2 → MatchEnd (round_count reached)
    ms.set_phase(MatchPhase::RoundEnd, 5.0F);
    assert(ms.tick(5.0F, rules));
    assert(ms.phase == MatchPhase::MatchEnd);

    std::cout << "test_match_state_round_transition passed.\n";
}

void test_match_state_full_lifecycle() {
    MatchState ms{};
    GameModeRules rules{};

    // Full cycle: Lobby → Warmup → Countdown → InProgress → MatchEnd → PostMatch → Lobby
    assert(ms.tick(0.0F, rules));        // Lobby → Warmup
    assert(ms.phase == MatchPhase::Warmup);

    assert(ms.tick(15.0F, rules));       // Warmup → Countdown
    assert(ms.phase == MatchPhase::Countdown);

    assert(ms.tick(3.0F, rules));        // Countdown → InProgress
    assert(ms.phase == MatchPhase::InProgress);
    assert(ms.is_playing());

    // Score to trigger end
    ms.add_score(Team::Red, 0, 50);
    assert(ms.tick(0.016F, rules));      // InProgress → MatchEnd
    assert(ms.phase == MatchPhase::MatchEnd);

    assert(ms.tick(15.0F, rules));       // MatchEnd → PostMatch
    assert(ms.phase == MatchPhase::PostMatch);

    assert(ms.tick(30.0F, rules));       // PostMatch → Lobby
    assert(ms.phase == MatchPhase::Lobby);

    std::cout << "test_match_state_full_lifecycle passed.\n";
}

void test_match_state_add_score() {
    MatchState ms{};

    ms.add_score(Team::Red, 1, 5);
    assert(ms.team_score_red == 5);
    assert(ms.team_score_blue == 0);
    assert(ms.individual_score[1] == 5);
    assert(ms.match_winner_id == 1);

    ms.add_score(Team::Blue, 3, 6);
    assert(ms.team_score_blue == 6);
    assert(ms.individual_score[3] == 6);
    assert(ms.match_winner_id == 3);

    // FFA / no team: individual only
    ms.add_score(Team::FFA, 7, 2);
    assert(ms.individual_score[7] == 2);

    std::cout << "test_match_state_add_score passed.\n";
}

// ===================================================================
//  Weapon Definition  (91)
// ===================================================================

void test_weapon_definition_defaults() {
    WeaponDefinition def{};
    assert(def.magazine_size == 30);
    assert(def.base_damage == 25.0F);
    assert(def.headshot_multiplier == 2.0F);
    assert(def.fire_mode == FireMode::Hitscan);
    assert(def.slot == WeaponSlot::Primary);
    std::cout << "test_weapon_definition_defaults passed.\n";
}

void test_weapon_state_can_fire() {
    WeaponState ws{};
    ws.ammo_in_magazine = 10;

    // Ready
    assert(ws.can_fire());

    // Reloading blocks fire
    ws.is_reloading = true;
    assert(!ws.can_fire());
    ws.is_reloading = false;

    // Equipping blocks fire
    ws.is_equipping = true;
    assert(!ws.can_fire());
    ws.is_equipping = false;

    // Charging blocks fire
    ws.is_charging = true;
    assert(!ws.can_fire());
    ws.is_charging = false;

    // Cooldown blocks fire
    ws.fire_cooldown = 0.1F;
    assert(!ws.can_fire());
    ws.fire_cooldown = 0.0F;

    // No ammo blocks fire
    ws.ammo_in_magazine = 0;
    assert(!ws.can_fire());

    std::cout << "test_weapon_state_can_fire passed.\n";
}

void test_weapon_state_can_reload() {
    WeaponState ws{};
    ws.ammo_in_magazine = 5;  // partial
    ws.reserve_ammo = 10;     // has reserve ammo
    ws.magazine_capacity = 30;

    // Basic: can reload (ammo not full, has reserve)
    assert(ws.can_reload());

    ws.is_reloading = true;
    assert(!ws.can_reload());
    ws.is_reloading = false;

    ws.is_equipping = true;
    assert(!ws.can_reload());
    ws.is_equipping = false;

    std::cout << "test_weapon_state_can_reload passed.\n";
}

void test_weapon_runtime_defaults() {
    WeaponRuntime runtime{};
    runtime.reset();
    assert(runtime.active_weapon_index() == 0);
    assert(runtime.state().ammo_in_magazine == kWeaponRegistry[0].magazine_size);
    assert(runtime.state().magazine_capacity == kWeaponRegistry[0].magazine_size);
    assert(runtime.state().reserve_ammo == kWeaponRegistry[0].magazine_size * 3);
    assert(runtime.can_fire());
    std::cout << "test_weapon_runtime_defaults passed.\n";
}

void test_weapon_runtime_reload_cycle() {
    WeaponRuntime runtime{};
    runtime.reset();
    runtime.state().ammo_in_magazine = 5;
    runtime.state().reserve_ammo = 10;
    runtime.state().magazine_capacity = 30;
    runtime.start_reload();
    assert(runtime.state().is_reloading);
    runtime.tick(2.0F);
    assert(!runtime.state().is_reloading);
    assert(runtime.state().ammo_in_magazine == 15);
    assert(runtime.state().reserve_ammo == 0);
    std::cout << "test_weapon_runtime_reload_cycle passed.\n";
}

void test_weapon_runtime_equip() {
    WeaponRuntime runtime{};
    runtime.reset();
    runtime.equip(1);
    assert(runtime.active_weapon_index() == 1);
    assert(runtime.state().ammo_in_magazine == kWeaponRegistry[1].magazine_size);
    assert(runtime.state().magazine_capacity == kWeaponRegistry[1].magazine_size);
    std::cout << "test_weapon_runtime_equip passed.\n";
}

void test_loadout_defaults() {
    Loadout lo{};
    assert(lo.active_slot == 0);
    for (int i = 0; i < static_cast<int>(WeaponSlot::Count); ++i) {
        assert(lo.weapons[i] == 0);
    }
    std::cout << "test_loadout_defaults passed.\n";
}

void test_player_loadout_selection() {
    PlayerLoadoutSelection sel{};
    sel.primary_weapon_index = 2;
    sel.secondary_weapon_index = 5;
    sel.melee_weapon_index = 8;
    assert(sel.primary_weapon_index == 2);
    assert(sel.secondary_weapon_index == 5);
    assert(sel.melee_weapon_index == 8);
    std::cout << "test_player_loadout_selection passed.\n";
}

void test_player_reset_and_spawn() {
    Player player{};
    player.reset();

    PlayerSpawnDefinition spawn{};
    spawn.position = {4.0F, 2.0F, -3.0F};
    spawn.yaw = 90.0F;

    player.reset_to_spawn(spawn);

    assert(player.state().position.x == 4.0F);
    assert(player.state().position.y == 2.0F);
    assert(player.state().position.z == -3.0F);
    assert(player.state().yaw == 90.0F);
    assert(player.state().health == 100.0F);
    assert(player.state().shield == 100.0F);
    assert(player.loadout().active_slot == 0);
    assert(player.get_active_weapon_index() == 0);
    assert(player.get_reserve_ammo() == 150);
    std::cout << "test_player_reset_and_spawn passed.\n";
}

void test_player_switch_weapon_and_damage() {
    Player player{};
    player.reset();

    player.switch_weapon(static_cast<int>(WeaponSlot::Secondary));
    assert(player.loadout().active_slot == static_cast<int>(WeaponSlot::Secondary));
    assert(player.get_active_weapon_index() == 1);
    assert(player.get_ammo_current() == kWeaponRegistry[1].magazine_size);

    const float actual_damage = player.apply_damage(30.0F);
    assert(actual_damage > 10.0F && actual_damage < 11.0F);
    assert(player.state().health < 100.0F);
    assert(player.state().shield < 100.0F);
    std::cout << "test_player_switch_weapon_and_damage passed.\n";
}

// ===================================================================
//  Status Effects  (94)
// ===================================================================

void test_status_effect_defaults() {
    StatusEffectInstance se{};
    assert(se.type == StatusEffectType::None);
    assert(se.duration_remaining == 0.0F);
    assert(se.tick_interval == 1.0F);
    assert(se.magnitude == 1.0F);
    std::cout << "test_status_effect_defaults passed.\n";
}

void test_status_effect_types() {
    assert(static_cast<int>(StatusEffectType::None) == 0);
    assert(static_cast<int>(StatusEffectType::Cloaked) == 8);
    assert(static_cast<int>(StatusEffectType::Count) == 9);
    std::cout << "test_status_effect_types passed.\n";
}

// ===================================================================
//  Replay Frame  (100)
// ===================================================================

void test_replay_frame_header() {
    ReplayFrameHeader hdr{};
    assert(hdr.tick == 0);
    assert(hdr.delta_seconds > 0.0F);
    assert(hdr.input_command_count == 0);
    assert(hdr.event_count == 0);

    hdr.tick = 100;
    hdr.event_count = 3;
    assert(hdr.tick == 100);
    assert(hdr.event_count == 3);

    std::cout << "test_replay_frame_header passed.\n";
}

}  // namespace

int main() {
    // Team / Game Mode
    test_team_enums();
    test_can_damage_team_mode();
    test_game_mode_rules_defaults();

    // Spawn System
    test_spawn_selector_basic();
    test_spawn_selector_avoids_recent();
    test_spawn_selector_disabled();
    test_spawn_selector_empty();

    // Damage Model
    test_damage_basic_no_armor();
    test_damage_lethal();
    test_damage_with_armor();
    test_damage_armor_depleted();
    test_damage_headshot();
    test_armor_config_defaults();

    // Match State Machine
    test_match_state_initial();
    test_match_state_progression();
    test_match_state_score_limit();
    test_match_state_time_limit();
    test_match_state_overtime();
    test_match_state_round_transition();
    test_match_state_full_lifecycle();
    test_match_state_add_score();

    // Weapon Definitions
    test_weapon_definition_defaults();
    test_weapon_state_can_fire();
    test_weapon_state_can_reload();
    test_weapon_runtime_defaults();
    test_weapon_runtime_reload_cycle();
    test_weapon_runtime_equip();
    test_loadout_defaults();
    test_player_loadout_selection();
    test_player_reset_and_spawn();
    test_player_switch_weapon_and_damage();

    // Status Effects
    test_status_effect_defaults();
    test_status_effect_types();

    // Replay
    test_replay_frame_header();

    std::cout << "\nAll gameplay types tests passed.\n";
    return 0;
}

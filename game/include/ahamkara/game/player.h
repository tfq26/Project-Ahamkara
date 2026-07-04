#pragma once

#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/weapon_runtime.h"
#include "ahamkara/game/worlds/world_definition.h"

namespace ahamkara::game {

/// Player-owned runtime state that should not live in World.
///
/// This is the sole owner of the player snapshot, armor configuration,
/// loadout, and active weapon runtime.  World accesses weapon state through
/// Player, never directly owning it.  Weapon presentation (viewmodels,
/// animations, model cache) lives in ae::render and the client layer — not
/// here.
///
/// Intentionally does not own buffs, debuffs, quests, or meta/progression
/// stats.
class Player {
public:
    Player();

    void reset();
    void reset_to_spawn(const PlayerSpawnDefinition& spawn);
    void set_state(const ReplicatedPlayerState& state) { state_ = state; }

    [[nodiscard]] const ReplicatedPlayerState& state() const { return state_; }
    [[nodiscard]] ReplicatedPlayerState& state() { return state_; }

    [[nodiscard]] const Loadout& loadout() const { return loadout_; }
    [[nodiscard]] Loadout& loadout() { return loadout_; }

    [[nodiscard]] const ArmorConfig& armor_config() const { return armor_config_; }
    void set_armor_config(const ArmorConfig& armor) { armor_config_ = armor; }

    [[nodiscard]] bool is_alive() const { return state_.health > 0.0F; }
    void reset_weapon_runtime(int definition_index = 0, int reserve_ammo_override = -1);

    void switch_weapon(int slot);
    void start_reload();
    bool consume_ammo();
    void tick_weapon(float delta_seconds);
    [[nodiscard]] bool can_fire() const { return weapon_runtime_.can_fire(); }
    /// Notify the weapon runtime that a shot was fired.
    /// Delegates to WeaponRuntime::notify_fired() → on_fire().
    void notify_weapon_fired() { weapon_runtime_.notify_fired(); }

    [[nodiscard]] int get_ammo_current() const { return weapon_runtime_.state().ammo_in_magazine; }
    [[nodiscard]] int get_ammo_max() const { return weapon_runtime_.state().magazine_capacity; }
    [[nodiscard]] int get_reserve_ammo() const { return weapon_runtime_.state().reserve_ammo; }
    [[nodiscard]] int get_active_weapon_index() const { return weapon_runtime_.active_weapon_index(); }
    [[nodiscard]] const WeaponDefinition& get_active_weapon_def() const { return weapon_runtime_.active_weapon_def(); }
    [[nodiscard]] const WeaponState& get_weapon_state() const { return weapon_runtime_.state(); }

    [[nodiscard]] float fire_cooldown_timer() const { return weapon_runtime_.fire_cooldown(); }
    void set_fire_cooldown_timer(float seconds) { weapon_runtime_.set_fire_cooldown(seconds); }

    /// Apply damage and return the actual health damage taken after armor.
    float apply_damage(float damage);

private:
    ReplicatedPlayerState state_ {};
    Loadout loadout_ {};
    ArmorConfig armor_config_ {};
    WeaponRuntime weapon_runtime_ {};
};

}  // namespace ahamkara::game

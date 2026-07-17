#pragma once

#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/weapon_runtime.h"
#include "ahamkara/game/worlds/world_definition.h"

namespace ahamkara::game {

/// Runtime state for player-owned abilities (melee, grenade, class abilities).
struct AbilityState {
    float grenade_cooldown {0.0F};   // 0 = ready to use
    int grenade_count {2};            // max 2 grenades carried
    float special_cooldown {0.0F};   // class ability cooldown (e.g. dodge/barrier)
    float artifact_cooldown {0.0F};  // artifact ability cooldown
    float ultimate_charge {0.0F};    // 0.0-1.0 — builds from combat actions
    float energy {100.0F};           // shared resource pool
    float max_energy {100.0F};

    static constexpr float kGrenadeCooldownTime = 12.0F;
    static constexpr float kSpecialCooldownTime = 20.0F;
    static constexpr float kArtifactCooldownTime = 45.0F;

    void tick(float dt) {
        grenade_cooldown = std::max(0.0F, grenade_cooldown - dt);
        special_cooldown = std::max(0.0F, special_cooldown - dt);
        artifact_cooldown = std::max(0.0F, artifact_cooldown - dt);
        // Energy regen
        energy = std::min(max_energy, energy + 5.0F * dt);
        // Ultimate charge gains from combat (caller increments via add_ultimate_charge)
    }

    /// Try to use a grenade.  Returns true if one was available and consumed.
    bool use_grenade() {
        if (grenade_count <= 0 || grenade_cooldown > 0.0F) return false;
        grenade_count--;
        grenade_cooldown = kGrenadeCooldownTime;
        add_ultimate_charge(0.05F);
        return true;
    }

    /// Try to use a class ability.  Returns true if off cooldown and enough energy.
    bool use_special() {
        if (special_cooldown > 0.0F || energy < 30.0F) return false;
        energy -= 30.0F;
        special_cooldown = kSpecialCooldownTime;
        add_ultimate_charge(0.10F);
        return true;
    }

    void add_ultimate_charge(float amount) {
        ultimate_charge = std::min(1.0F, ultimate_charge + amount);
    }

    [[nodiscard]] bool grenade_available() const {
        return grenade_count > 0 && grenade_cooldown <= 0.0F;
    }
    [[nodiscard]] bool special_available() const {
        return special_cooldown <= 0.0F && energy >= 30.0F;
    }
    [[nodiscard]] bool ultimate_ready() const {
        return ultimate_charge >= 1.0F;
    }
};

/// Player-owned runtime state that should not live in World.
///
/// This is the sole owner of the player snapshot, armor configuration,
/// loadout, and active weapon runtime.  World accesses weapon state through
/// Player, never directly owning it.  Weapon presentation (viewmodels,
/// animations, model cache) lives in ae::render and the client layer — not
/// here.
///
/// ## Weapon Ownership Chain
///
/// Player owns `weapon_runtime_` (a `WeaponRuntime`).  All weapon state
/// (ammo, cooldowns, reload status) lives inside `WeaponRuntime`.  World
/// accesses weapon state through read-only accessors on `Player` — it NEVER
/// holds a mutable reference to `WeaponRuntime` or `WeaponState`.
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

    /// Tick ability cooldowns, energy regen, and ultimate charge.
    void tick_abilities(float dt) { ability_state_.tick(dt); }

    /// Use a grenade if available.  Returns true if consumed.
    bool use_grenade() { return ability_state_.use_grenade(); }
    /// Use a class ability if available.  Returns true if consumed.
    bool use_special() { return ability_state_.use_special(); }
    /// Add ultimate charge from combat actions.
    void add_ultimate_charge(float amount) { ability_state_.add_ultimate_charge(amount); }

    [[nodiscard]] const AbilityState& ability_state() const { return ability_state_; }
    [[nodiscard]] AbilityState& ability_state() { return ability_state_; }

    /// Apply damage and return the actual health damage taken after armor.
    float apply_damage(float damage);

private:
    ReplicatedPlayerState state_ {};
    Loadout loadout_ {};
    ArmorConfig armor_config_ {};
    WeaponRuntime weapon_runtime_ {};
    AbilityState ability_state_ {};
};

}  // namespace ahamkara::game

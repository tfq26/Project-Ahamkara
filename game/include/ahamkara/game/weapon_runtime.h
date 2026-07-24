#pragma once

#include "ahamkara/game/weapon_registry.h"

namespace ahamkara::game {

/// Base runtime seam for the player's active weapon.
///
/// Owns pure gameplay state: ammo count, reload timing, fire cooldown, and
/// equip status.  Presentation concerns (viewmodel meshes, animations, model
/// cache) belong outside this class — see ae::render::WeaponModelCache and
/// the client presentation layer.
///
/// ## Ownership
///
/// `WeaponRuntime` is owned by `Player` (ahamkara::game::Player).  `World`
/// accesses weapon state only through `Player` read-accessors.  Weapon state
/// never lives in `World` directly.
///
/// ## Subclass Extension Points
///
/// Override the virtual hooks below to add weapon-specific runtime behavior
/// without touching presentation or caching code:
///
///   on_equipped(prev, next)   — react to a weapon switch
///   on_reload_finished()      — apply reload-side effects
///   on_fire()                 — add per-shot behavior (e.g. charge drain,
///                               alt-ammo consumption, spread accumulation)
///   on_tick(dt)               — per-frame runtime logic (e.g. charge-up,
///                               overheat cooldown)
///
/// ## Subclass Contract
///
/// 1. Override zero or more of the virtual hooks above.
/// 2. Read `current_def()` or `state()` to inspect current weapon parameters.
/// 3. Mutate `state()` to drive custom runtime state (charge level, overheat
///    percentage, spread multiplier).
/// 4. NEVER include render or animation headers.
/// 5. NEVER store viewmodel, attachment, or animation state here.
/// 6. NEVER access WeaponModelCache or any ae::render type.
///
class WeaponRuntime {
public:
    WeaponRuntime() = default;
    virtual ~WeaponRuntime() = default;

    void reset(int definition_index = 0);
    void equip(int definition_index);
    void tick(float delta_seconds);
    void start_reload();
    [[nodiscard]] bool consume_ammo();

    [[nodiscard]] bool can_fire() const;
    [[nodiscard]] bool can_reload() const;

    [[nodiscard]] int active_weapon_index() const { return state_.definition_index; }
    [[nodiscard]] const WeaponDefinition& active_weapon_def() const;

    [[nodiscard]] const WeaponState& state() const { return state_; }
    [[nodiscard]] WeaponState& state() { return state_; }

    [[nodiscard]] float fire_cooldown() const { return state_.fire_cooldown; }
    void set_fire_cooldown(float seconds) { state_.fire_cooldown = seconds; }

    /// Called by the firing system after consume_ammo() succeeds.
    void notify_fired() { on_fire(); }

    /// --- Fire mode helpers ---

    /// Get the current fire mode of the active weapon.
    [[nodiscard]] FireMode fire_mode() const;

    /// Whether the trigger should fire this frame based on fire mode
    /// and previous trigger state.  For Single/Burst modes this handles
    /// edge detection (only fire on rising edge).
    /// Call once per frame from the firing system.
    /// @param trigger_held  true if the fire button/trigger is currently held.
    /// @return true if the weapon should fire this frame.
    bool trigger_pull(bool trigger_held);

    /// Combined can_fire + trigger_pull check.
    /// Returns true if the weapon can fire AND the trigger logic says to fire.
    /// This is the main entry point the firing system should use.
    /// @param trigger_held  true if the fire button/trigger is currently held.
    /// @return true if the weapon should fire this frame.
    bool try_fire(bool trigger_held);

    /// --- Burst fire ---

    /// Start a burst sequence (for Burst fire mode).
    void start_burst();

    /// Advance burst fire state.  Called from tick().
    void tick_burst(float delta_seconds);

    /// Whether a burst sequence is in progress.
    [[nodiscard]] bool is_bursting() const { return state_.is_bursting(); }

    /// How many rounds remain in the current burst.
    [[nodiscard]] int burst_rounds_remaining() const {
        return state_.burst_rounds_total - state_.burst_rounds_fired;
    }

    /// --- Deterministic recoil/spread generation ---

    /// Get a deterministic recoil offset for the current shot.
    /// Returns the (yaw, pitch) offset from the weapon's pattern.
    [[nodiscard]] RecoilEntry generate_recoil();

    /// Get a deterministic spread offset for a given pellet index.
    /// Used by shotguns and multi-pellet weapons.
    /// @param pellet_index   Index of the pellet within the shot (0 = first).
    /// @param pellet_count   Total pellets in this shot.
    [[nodiscard]] float generate_spread(int pellet_index, int pellet_count) const;

    /// Advance the recoil seed by one shot (called after generating recoil).
    void advance_recoil_seed();

    /// Current accumulated spread angle (degrees).
    [[nodiscard]] float current_spread() const { return state_.current_spread; }

    /// Reset recoil seed (called on weapon switch).
    void reset_recoil_seed() { state_.recoil_seed = 0; }

    /// --- Trigger state tracking ---
    /// Set to true when the trigger was pressed last frame (for edge detection).
    void set_trigger_was_pressed(bool pressed) { trigger_was_pressed_ = pressed; }
    [[nodiscard]] bool trigger_was_pressed() const { return trigger_was_pressed_; }

protected:
    // -- Subclass hooks (default no-op) --

    /// Called after equip() applies a new weapon definition.
    virtual void on_equipped(int previous_index, int new_index) {}

    /// Called after the reload timer expires and ammo has been refilled.
    virtual void on_reload_finished() {}

    /// Called every frame from tick() after cooldown/reload processing.
    virtual void on_tick(float delta_seconds) { (void)delta_seconds; }

    /// Called when the weapon is fired (after consume_ammo).
    /// Override to add per-shot runtime logic such as charge drain,
    /// alt-ammo consumption, or spread accumulation.
    virtual void on_fire() {}

    /// Called when a burst sequence finishes (all rounds fired or interrupted).
    virtual void on_burst_finished() {}

    /// Read the active WeaponDefinition from subclass hooks without
    /// going through the public API.
    [[nodiscard]] const WeaponDefinition& current_def() const {
        return active_weapon_def();
    }

    /// Current reload countdown (seconds remaining).  Zero when not reloading.
    [[nodiscard]] float reload_timer() const { return reload_timer_; }

private:
    void apply_definition(int definition_index);

    WeaponState state_ {};
    float reload_timer_ {0.0F};
    bool trigger_was_pressed_ {false};
    bool pending_burst_round_ {false};  ///< Set by tick_burst when next burst round is ready.

    /// Deterministic hash for recoil/spread generation.
    static int hash_seed(int seed, int index);
};

}  // namespace ahamkara::game

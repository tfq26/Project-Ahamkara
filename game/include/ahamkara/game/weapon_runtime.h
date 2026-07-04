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
/// ## What Subclasses Must NOT Do
///
/// - Do not include render or animation headers.
/// - Do not store viewmodel, attachment, or animation state here.
/// - Do not access WeaponModelCache or any ae::render type.
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

    /// Current reload countdown (seconds remaining).  Zero when not reloading.
    [[nodiscard]] float reload_timer() const { return reload_timer_; }

private:
    void apply_definition(int definition_index);

    WeaponState state_ {};
    float reload_timer_ {0.0F};
};

}  // namespace ahamkara::game

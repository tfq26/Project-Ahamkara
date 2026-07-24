#include "ahamkara/game/weapon_runtime.h"

#include <algorithm>
#include <cmath>

namespace ahamkara::game {

// --- Deterministic hash helpers ----------------------------------------------

int WeaponRuntime::hash_seed(int seed, int index) {
    // Simple deterministic hash: no std::rand(), no external state.
    // Uses Boole's rule prime multipliers for good distribution.
    return (seed * 73856093) ^ (index * 19349663) ^ (index * 83492791);
}

// --- Lifecycle ---------------------------------------------------------------

void WeaponRuntime::reset(int definition_index) {
    state_ = {};
    reload_timer_ = 0.0F;
    trigger_was_pressed_ = false;
    pending_burst_round_ = false;
    apply_definition(definition_index);
}

void WeaponRuntime::equip(int definition_index) {
    const int previous_index = state_.definition_index;
    if (definition_index < 0 || static_cast<std::size_t>(definition_index) >= kWeaponRegistrySize) {
        return;
    }
    if (definition_index == previous_index) {
        return;
    }

    apply_definition(definition_index);
    on_equipped(previous_index, definition_index);
}

// --- Tick ---

void WeaponRuntime::tick(float delta_seconds) {
    // Fire cooldown decay
    if (state_.fire_cooldown > 0.0F) {
        state_.fire_cooldown = std::max(0.0F, state_.fire_cooldown - delta_seconds);
    }

    // Spread recovery toward base
    {
        const auto& def = active_weapon_def();
        float base_spread = def.spread_angle;
        float recovery_rate = (def.spread_recovery > 0.0F) ? def.spread_recovery : 10.0F; // default 10 deg/s
        if (state_.current_spread > base_spread) {
            state_.current_spread = std::max(base_spread, state_.current_spread - recovery_rate * delta_seconds);
        } else if (state_.current_spread < base_spread) {
            state_.current_spread = std::min(base_spread, state_.current_spread + recovery_rate * delta_seconds);
        }
    }

    // Reload timer
    if (state_.is_reloading) {
        reload_timer_ = std::max(0.0F, reload_timer_ - delta_seconds);
        if (reload_timer_ <= 0.0F) {
            const int needed = state_.magazine_capacity - state_.ammo_in_magazine;
            const int available = std::min(needed, state_.reserve_ammo);
            state_.ammo_in_magazine += available;
            state_.reserve_ammo -= available;
            state_.is_reloading = false;
            reload_timer_ = 0.0F;
            on_reload_finished();
        }
    }

    // Burst timer (advances between rounds within a burst)
    if (state_.is_bursting()) {
        tick_burst(delta_seconds);
    }

    on_tick(delta_seconds);
}

// --- Reload ------------------------------------------------------------------

void WeaponRuntime::start_reload() {
    if (can_reload()) {
        state_.is_reloading = true;
        reload_timer_ = active_weapon_def().reload_time_s;

        // Interrupt any in-progress burst
        if (state_.is_bursting()) {
            state_.burst_rounds_fired = 0;
            state_.burst_timer = 0.0F;
            pending_burst_round_ = false;
            on_burst_finished();
        }
    }
}

bool WeaponRuntime::consume_ammo() {
    if (state_.ammo_in_magazine <= 0) {
        return false;
    }
    state_.ammo_in_magazine--;
    return true;
}

// --- Fire mode helpers -------------------------------------------------------

FireMode WeaponRuntime::fire_mode() const {
    return active_weapon_def().fire_mode;
}

bool WeaponRuntime::try_fire(bool trigger_held) {
    // For burst mode: the burst_timer inside WeaponRuntime is the rate limiter,
    // NOT the fire_cooldown. Clear the cooldown for burst rounds so the firing
    // system can fire without being blocked by a stale cooldown from a previous
    // round. Without this, the fire functions (fire_hitscan etc.) set cooldown
    // to fire_interval() which is too long for intra-burst spacing.
    if (fire_mode() == FireMode::Burst && state_.is_bursting()) {
        state_.fire_cooldown = 0.0F;
    }

    if (!state_.can_fire()) {
        return false;
    }
    return trigger_pull(trigger_held);
}

bool WeaponRuntime::trigger_pull(bool trigger_held) {
    // Check for pending burst round FIRST — this works in any fire mode.
    // The burst system is orthogonal to fire mode: tick_burst() sets this flag
    // when the burst timer expires, and we fire the next round regardless of
    // whether the weapon is in Single, Burst, or Auto mode.
    if (pending_burst_round_) {
        pending_burst_round_ = false;
        state_.burst_rounds_fired++;

        if (state_.is_burst_complete()) {
            // Burst complete — reset for next burst
            state_.burst_rounds_fired = 0;
            on_burst_finished();
        } else {
            // Schedule next round
            const auto& def = active_weapon_def();
            float interval = (def.burst_interval > 0.0F) ? def.burst_interval : def.fire_interval();
            state_.burst_timer = interval;
        }
        trigger_was_pressed_ = trigger_held;
        return true;
    }

    const FireMode mode = fire_mode();

    switch (mode) {
        case FireMode::Single:
            // Edge-triggered: fire only on the frame the trigger is pressed.
            if (trigger_held && !trigger_was_pressed_) {
                trigger_was_pressed_ = trigger_held;
                return true;
            }
            trigger_was_pressed_ = trigger_held;
            return false;

        case FireMode::Burst: {
            // Start burst on rising edge
            if (trigger_held && !trigger_was_pressed_) {
                trigger_was_pressed_ = trigger_held;
                if (!state_.is_bursting()) {
                    start_burst();
                    return true; // first round of burst fires immediately
                }
                return false;
            }
            trigger_was_pressed_ = trigger_held;
            return false;
        }

        case FireMode::Auto:
            // Level-triggered: fire every frame while held (rate-limited by cooldown).
            trigger_was_pressed_ = trigger_held;
            return trigger_held;

        default:
            trigger_was_pressed_ = trigger_held;
            return trigger_held;
    }
}

// --- Burst fire --------------------------------------------------------------

void WeaponRuntime::start_burst() {
    state_.burst_rounds_fired = 1; // first round counted (fires immediately)
    const auto& def = active_weapon_def();
    state_.burst_rounds_total = (def.burst_rounds > 1) ? def.burst_rounds : 3;
    // Burst interval: use explicit burst_interval, or fall back to fire_interval
    float interval = (def.burst_interval > 0.0F) ? def.burst_interval : def.fire_interval();
    state_.burst_timer = interval;
    pending_burst_round_ = false;
}

void WeaponRuntime::tick_burst(float delta_seconds) {
    if (!state_.is_bursting()) {
        return;
    }

    state_.burst_timer -= delta_seconds;

    // When timer expires, set the pending flag so trigger_pull() fires the next round.
    if (state_.burst_timer <= 0.0F && state_.is_bursting()) {
        pending_burst_round_ = true;
    }
}

// --- Recoil / spread generation ----------------------------------------------

RecoilEntry WeaponRuntime::generate_recoil() {
    const auto& def = active_weapon_def();
    RecoilEntry entry {0.0F, 0.0F};

    if (!def.recoil_pattern.empty()) {
        // Deterministic: index into pattern using the recoil seed.
        std::size_t idx = static_cast<std::size_t>(state_.recoil_seed) % def.recoil_pattern.size();
        const auto& pattern_entry = def.recoil_pattern[idx];
        entry.pitch = pattern_entry.pitch;
        entry.yaw = pattern_entry.yaw;
    }

    // Advance seed for next shot
    advance_recoil_seed();

    return entry;
}

float WeaponRuntime::generate_spread(int pellet_index, int pellet_count) const {
    if (pellet_count <= 1) {
        return 0.0F; // single-pellet weapons don't get per-pellet spread
    }

    // Deterministic offset based on recoil seed and pellet index.
    // Uses the same hash function as world_projectile.cpp for consistency.
    const int hash = hash_seed(state_.recoil_seed, pellet_index);
    const float normalized = static_cast<float>(hash & 0x7FFFFFFF) / 1073741824.0F; // / 2^30
    return (normalized - 0.5F) * 2.0F * state_.current_spread;
}

void WeaponRuntime::advance_recoil_seed() {
    ++state_.recoil_seed;

    // When firing, accumulate spread per shot.
    const auto& def = active_weapon_def();
    if (def.spread_per_shot > 0.0F) {
        state_.current_spread = std::min(
            state_.current_spread + def.spread_per_shot,
            def.spread_angle + def.spread_per_shot * 10.0F // cap at 10x per-shot
        );
    }
}

// --- Capability checks -------------------------------------------------------

bool WeaponRuntime::can_fire() const {
    return state_.can_fire();
}

bool WeaponRuntime::can_reload() const {
    return state_.can_reload();
}

// --- Registry access ---------------------------------------------------------

const WeaponDefinition& WeaponRuntime::active_weapon_def() const {
    const int idx = state_.definition_index;
    if (idx >= 0 && static_cast<std::size_t>(idx) < kWeaponRegistrySize) {
        return kWeaponRegistry[static_cast<std::size_t>(idx)];
    }
    return kWeaponRegistry[0];
}

// --- Internal ----------------------------------------------------------------

void WeaponRuntime::apply_definition(int definition_index) {
    if (definition_index < 0 || static_cast<std::size_t>(definition_index) >= kWeaponRegistrySize) {
        definition_index = 0;
    }

    state_.definition_index = definition_index;
    const auto& def = kWeaponRegistry[static_cast<std::size_t>(definition_index)];
    state_.magazine_capacity = def.magazine_size;
    state_.ammo_in_magazine = def.magazine_size;
    const int reserve = (def.reserve_ammo_max > 0) ? def.reserve_ammo_max : (def.magazine_size * 3);
    state_.reserve_ammo = reserve;
    state_.fire_cooldown = 0.0F;
    state_.is_reloading = false;
    state_.is_equipping = false;
    state_.is_charging = false;

    // Reset burst state
    state_.burst_rounds_fired = 0;
    state_.burst_rounds_total = (def.burst_rounds > 0) ? def.burst_rounds : 3;
    state_.burst_timer = 0.0F;

    // Reset recoil/spread state
    state_.recoil_seed = 0;
    state_.current_spread = def.spread_angle;
    state_.spread_angle_base = def.spread_angle;

    reload_timer_ = 0.0F;
    trigger_was_pressed_ = false;
    pending_burst_round_ = false;
}

}  // namespace ahamkara::game

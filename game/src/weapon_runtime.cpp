#include "ahamkara/game/weapon_runtime.h"

#include <algorithm>

namespace ahamkara::game {

void WeaponRuntime::reset(int definition_index) {
    state_ = {};
    reload_timer_ = 0.0F;
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

void WeaponRuntime::tick(float delta_seconds) {
    if (state_.fire_cooldown > 0.0F) {
        state_.fire_cooldown = std::max(0.0F, state_.fire_cooldown - delta_seconds);
    }

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

    on_tick(delta_seconds);
}

void WeaponRuntime::start_reload() {
    if (can_reload()) {
        state_.is_reloading = true;
        reload_timer_ = active_weapon_def().reload_time_s;
    }
}

bool WeaponRuntime::consume_ammo() {
    if (state_.ammo_in_magazine <= 0) {
        return false;
    }
    state_.ammo_in_magazine--;
    return true;
}

bool WeaponRuntime::can_fire() const {
    return state_.can_fire();
}

bool WeaponRuntime::can_reload() const {
    return state_.can_reload();
}

const WeaponDefinition& WeaponRuntime::active_weapon_def() const {
    const int idx = state_.definition_index;
    if (idx >= 0 && static_cast<std::size_t>(idx) < kWeaponRegistrySize) {
        return kWeaponRegistry[static_cast<std::size_t>(idx)];
    }
    return kWeaponRegistry[0];
}

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
    reload_timer_ = 0.0F;
}

}  // namespace ahamkara::game

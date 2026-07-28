#include "ahamkara/game/player.h"

#include <algorithm>

namespace ahamkara::game {

Player::Player() {
    reset();
}

void Player::reset() {
    state_ = {};
    loadout_ = {};
    loadout_.weapons[static_cast<int>(WeaponSlot::Primary)] = 0;
    loadout_.weapons[static_cast<int>(WeaponSlot::Secondary)] = 1;
    loadout_.weapons[static_cast<int>(WeaponSlot::Melee)] = 2;
    armor_config_ = {};
    inventory_.clear();
    progression_ = {};
    currency_ = {};
    reset_weapon_runtime(0, 150);
}

void Player::reset_to_spawn(const PlayerSpawnDefinition& spawn) {
    state_.position = spawn.position;
    state_.velocity = {};
    state_.yaw = spawn.yaw;
    state_.movement_state = MovementState::Idle;
    state_.health = 100.0F;
    state_.shield = 100.0F;
}

void Player::reset_weapon_runtime(int definition_index, int reserve_ammo_override) {
    weapon_runtime_.reset(definition_index);
    if (reserve_ammo_override >= 0) {
        weapon_runtime_.state().reserve_ammo = reserve_ammo_override;
    }
}

void Player::switch_weapon(int slot) {
    const int idx = static_cast<int>(slot);
    if (idx < 0 || idx >= static_cast<int>(WeaponSlot::Count)) {
        return;
    }

    const int def_idx = loadout_.weapons[idx];
    if (def_idx < 0 || static_cast<std::size_t>(def_idx) >= kWeaponRegistrySize) {
        return;
    }
    if (def_idx == weapon_runtime_.active_weapon_index()) {
        return;
    }

    weapon_runtime_.equip(def_idx);
    loadout_.active_slot = idx;
}

void Player::start_reload() {
    weapon_runtime_.start_reload();
}

bool Player::consume_ammo() {
    return weapon_runtime_.consume_ammo();
}

void Player::tick_weapon(float delta_seconds) {
    weapon_runtime_.tick(delta_seconds);
}

float Player::apply_damage(float damage) {
    if (!is_alive()) {
        return 0.0F;
    }

    float actual_damage = damage;
    if (state_.shield > 0.0F) {
        float armor_dmg = actual_damage * armor_config_.absorption_fraction;
        if (armor_dmg > state_.shield) {
            armor_dmg = state_.shield;
        }
        state_.shield -= armor_dmg;
        actual_damage = damage - armor_dmg;
    }

    state_.health -= actual_damage;
    if (state_.health < 0.0F) {
        state_.health = 0.0F;
    }

    return actual_damage;
}

}  // namespace ahamkara::game

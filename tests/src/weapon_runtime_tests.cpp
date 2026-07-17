#include "ahamkara/game/weapon_runtime.h"
#include "ahamkara/game/weapon_registry.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

using namespace ahamkara::game;

// ----- Basic lifecycle -------------------------------------------------------

void test_construct_and_reset() {
    WeaponRuntime rt;
    assert(rt.active_weapon_index() == 0); // default index after value-init

    rt.reset(0); // AR-15
    assert(rt.active_weapon_index() == 0);
    assert(rt.state().ammo_in_magazine == 50);
    assert(rt.state().magazine_capacity == 50);
    assert(rt.state().reserve_ammo == 150);
    assert(!rt.state().is_reloading);
    assert(rt.can_fire());
    assert(!rt.can_reload()); // full magazine
    std::cout << "test_construct_and_reset passed.\n";
}

void test_reset_to_shotgun() {
    WeaponRuntime rt;
    rt.reset(1); // shotgun — slot 1
    assert(rt.active_weapon_index() == 1);
    assert(rt.state().ammo_in_magazine == 8);
    assert(rt.state().magazine_capacity == 8);
    assert(rt.state().reserve_ammo == 32);
    assert(std::string(weapon_name(1)) == "Shotgun");
    std::cout << "test_reset_to_shotgun passed.\n";
}

void test_reset_to_rocket_launcher() {
    WeaponRuntime rt;
    rt.reset(2);
    assert(rt.active_weapon_index() == 2);
    assert(rt.state().ammo_in_magazine == 1);
    assert(rt.state().reserve_ammo == 6);
    std::cout << "test_reset_to_rocket_launcher passed.\n";
}

void test_equip_switch() {
    WeaponRuntime rt;
    rt.reset(0);
    assert(rt.active_weapon_index() == 0);

    rt.equip(1);
    assert(rt.active_weapon_index() == 1);
    assert(rt.state().ammo_in_magazine == 8);

    // Equip same index — no-op
    rt.equip(1);
    assert(rt.active_weapon_index() == 1);

    std::cout << "test_equip_switch passed.\n";
}

void test_equip_invalid_index_ignored() {
    WeaponRuntime rt;
    rt.reset(0);
    rt.equip(100); // out of bounds
    assert(rt.active_weapon_index() == 0);
    rt.equip(-1);
    assert(rt.active_weapon_index() == 0);
    std::cout << "test_equip_invalid_index_ignored passed.\n";
}

// ----- Ammo and reload -------------------------------------------------------

void test_consume_ammo() {
    WeaponRuntime rt;
    rt.reset(0);
    assert(rt.state().ammo_in_magazine == 50);

    bool ok = rt.consume_ammo();
    assert(ok);
    assert(rt.state().ammo_in_magazine == 49);

    // Exhaust magazine and try again
    for (int i = 0; i < 49; ++i) {
        bool consumed = rt.consume_ammo();
        (void)consumed;
    }
    assert(rt.state().ammo_in_magazine == 0);
    assert(!rt.can_fire());

    ok = rt.consume_ammo();
    assert(!ok);
    assert(rt.state().ammo_in_magazine == 0);

    std::cout << "test_consume_ammo passed.\n";
}

void test_reload_full_cycle() {
    WeaponRuntime rt;
    rt.reset(0);
    // Fire a few rounds
    for (int i = 0; i < 3; ++i) {
        (void)rt.consume_ammo();
    }
    assert(rt.state().ammo_in_magazine == 47);
    assert(rt.can_reload());

    rt.start_reload();
    assert(rt.state().is_reloading);

    // Not yet finished
    assert(!rt.can_fire());

    // Tick past reload time
    rt.tick(2.5F); // AR-15 reload_time_s = 2.0
    assert(!rt.state().is_reloading);
    assert(rt.state().ammo_in_magazine == 50); // refilled from reserve
    assert(rt.state().reserve_ammo == 147);    // consumed 3

    std::cout << "test_reload_full_cycle passed.\n";
}

void test_can_reload_conditions() {
    WeaponRuntime rt;
    rt.reset(0);
    assert(!rt.can_reload()); // full magazine

    (void)rt.consume_ammo();
    assert(rt.can_reload()); // not full

    rt.start_reload();
    assert(!rt.can_reload()); // already reloading

    std::cout << "test_can_reload_conditions passed.\n";
}

void test_cannot_reload_without_reserve() {
    WeaponRuntime rt;
    rt.reset(0);
    // Drain reserve to 0 by exhaustively reloading
    // AR-15 has 50 round mag, 150 reserve, 2s reload
    // 3 full reloads: 3*50=150 reserve consumed — after that no reserve
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Empty the magazine
        for (int i = 0; i < 50; ++i) {
            if (rt.state().ammo_in_magazine > 0) {
                (void)rt.consume_ammo();
            }
        }
        rt.start_reload();
        rt.tick(5.0F);
    }
    // Now reserve should be 0
    assert(rt.state().reserve_ammo == 0);

    // Empty the magazine once more
    for (int i = 0; i < 50; ++i) {
        if (rt.state().ammo_in_magazine > 0) {
            (void)rt.consume_ammo();
        }
    }
    assert(!rt.can_reload());

    std::cout << "test_cannot_reload_without_reserve passed.\n";
}

// ----- Fire cooldown ---------------------------------------------------------

void test_fire_cooldown_blocks_firing() {
    WeaponRuntime rt;
    rt.reset(0);
    assert(rt.can_fire());

    rt.set_fire_cooldown(0.5F);
    assert(!rt.can_fire());

    rt.tick(0.3F);
    assert(!rt.can_fire());

    rt.tick(0.3F);
    assert(rt.can_fire());

    std::cout << "test_fire_cooldown_blocks_firing passed.\n";
}

// ----- Virtual hook spy ------------------------------------------------------

struct HookSpy final : WeaponRuntime {
    int equipped_count {0};
    int equipped_prev {-1};
    int equipped_next {-1};
    int reload_finished_count {0};
    int fire_count {0};
    int tick_count {0};

  protected:
    void on_equipped(int prev, int next) override {
        ++equipped_count;
        equipped_prev = prev;
        equipped_next = next;
    }
    void on_reload_finished() override {
        ++reload_finished_count;
    }
    void on_fire() override {
        ++fire_count;
    }
    void on_tick(float) override {
        ++tick_count;
    }
};

void test_hook_on_equipped() {
    HookSpy rt;
    rt.reset(0);
    int prev_count = rt.equipped_count;

    rt.equip(1);
    assert(rt.equipped_count == prev_count + 1);
    assert(rt.equipped_prev == 0);
    assert(rt.equipped_next == 1);

    rt.equip(2);
    assert(rt.equipped_count == prev_count + 2);
    assert(rt.equipped_prev == 1);
    assert(rt.equipped_next == 2);

    std::cout << "test_hook_on_equipped passed.\n";
}

void test_hook_on_reload_finished() {
    HookSpy rt;
    rt.reset(0);
    assert(rt.reload_finished_count == 0);

    (void)rt.consume_ammo();
    rt.start_reload();
    rt.tick(3.0F);
    assert(rt.reload_finished_count == 1);

    std::cout << "test_hook_on_reload_finished passed.\n";
}

void test_hook_on_fire() {
    HookSpy rt;
    rt.reset(0);
    assert(rt.fire_count == 0);

    // consume_ammo does NOT call on_fire — notify_fired does
    (void)rt.consume_ammo();
    assert(rt.fire_count == 0);

    rt.notify_fired();
    assert(rt.fire_count == 1);

    std::cout << "test_hook_on_fire passed.\n";
}

void test_hook_on_tick() {
    HookSpy rt;
    rt.reset(0);
    assert(rt.tick_count == 0);

    rt.tick(0.016F);
    assert(rt.tick_count == 1);

    rt.tick(0.016F);
    rt.tick(0.016F);
    assert(rt.tick_count == 3);

    std::cout << "test_hook_on_tick passed.\n";
}

// ----- current_def() protected accessor -------------------------------------

struct DefReader final : WeaponRuntime {
    [[nodiscard]] const WeaponDefinition& read_current_def() const {
        return current_def();
    }
};

void test_current_def_accessor() {
    DefReader rt;
    rt.reset(0);
    const auto& def = rt.read_current_def();
    assert(def.magazine_size == 50);
    assert(def.base_damage == 20.0F);
    assert(def.rpm == 400.0F);

    rt.reset(1);
    const auto& def2 = rt.read_current_def();
    assert(def2.magazine_size == 8);
    assert(def2.base_damage == 10.0F);

    std::cout << "test_current_def_accessor passed.\n";
}

} // namespace

int main() {
    // Lifecycle
    test_construct_and_reset();
    test_reset_to_shotgun();
    test_reset_to_rocket_launcher();
    test_equip_switch();
    test_equip_invalid_index_ignored();

    // Ammo & reload
    test_consume_ammo();
    test_reload_full_cycle();
    test_can_reload_conditions();
    test_cannot_reload_without_reserve();

    // Cooldown
    test_fire_cooldown_blocks_firing();

    // Virtual hooks
    test_hook_on_equipped();
    test_hook_on_reload_finished();
    test_hook_on_fire();
    test_hook_on_tick();

    // Protected accessor
    test_current_def_accessor();

    std::cout << "\nAll weapon runtime tests passed.\n";
    return 0;
}

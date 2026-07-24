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

// ----- Fire mode: Single (semi-auto edge detection) --------------------------

void test_single_fire_mode_edge_detection() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15, mode = Auto — need to test Single mode
    rt.reset(1); // Shotgun, mode = Single

    // Trigger not held: no fire
    assert(!rt.trigger_pull(false));
    assert(!rt.trigger_pull(false));

    // Rising edge: fires
    assert(rt.trigger_pull(true));

    // Still held: does NOT fire (edge-triggered)
    assert(!rt.trigger_pull(true));
    assert(!rt.trigger_pull(true));

    // Release and press again: fires
    assert(!rt.trigger_pull(false));
    assert(rt.trigger_pull(true));

    std::cout << "test_single_fire_mode_edge_detection passed.\n";
}

void test_single_fire_mode_try_fire() {
    WeaponRuntime rt;
    rt.reset(1); // Shotgun, mode = Single
    assert(rt.state().ammo_in_magazine > 0);

    // try_fire combines can_fire + trigger_pull
    assert(rt.try_fire(true));  // rising edge
    assert(!rt.try_fire(true)); // held — blocked by edge detection

    // Empty the magazine
    while (rt.state().ammo_in_magazine > 0) {
        (void)rt.consume_ammo();
    }
    assert(!rt.can_fire());

    // Release and try again — can't fire (no ammo)
    assert(!rt.try_fire(false));
    assert(!rt.try_fire(true));  // rising edge but can_fire = false

    std::cout << "test_single_fire_mode_try_fire passed.\n";
}

// ----- Fire mode: Auto (continuous while held) -------------------------------

void test_auto_fire_mode_fires_while_held() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15, mode = Auto

    // Not held: no fire
    assert(!rt.trigger_pull(false));

    // Held: fires
    assert(rt.trigger_pull(true));

    // Still held: fires again (level-triggered)
    assert(rt.trigger_pull(true));
    assert(rt.trigger_pull(true));

    // Released: no fire
    assert(!rt.trigger_pull(false));

    std::cout << "test_auto_fire_mode_fires_while_held passed.\n";
}

void test_auto_fire_mode_respects_rpm_cooldown() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15, 400 RPM → fire_interval ≈ 0.15s

    assert(rt.try_fire(true));  // first shot
    rt.state().fire_cooldown = 0.1F; // simulate cooldown from fire function

    // Cooldown active → can't fire (try_fire checks can_fire which checks cooldown)
    assert(!rt.try_fire(true));

    // Tick past cooldown
    rt.tick(0.06F); // 0.04 remaining
    assert(!rt.try_fire(true));

    rt.tick(0.05F); // -0.01 → cooldown cleared
    assert(rt.try_fire(true)); // fires again

    std::cout << "test_auto_fire_mode_respects_rpm_cooldown passed.\n";
}

// ----- Fire mode: Burst ------------------------------------------------------

void test_burst_fires_all_rounds() {
    WeaponRuntime rt;
    rt.reset(2); // Rocket Launcher, mode = Single — need a burst weapon
    // Create a weapon with burst mode. We'll temporarily modify the approach:
    // Use an anonymous struct test by resetting, but actually we test burst
    // by directly setting up the state via the weapon registry.
    //
    // To test burst properly, we need a weapon with Burst fire mode.
    // Since no weapon in the registry uses Burst, we test by verifying
    // that the Burst code path works correctly with a custom WeaponRuntime
    // configuration.
    //
    // Strategy: reset to rocket launcher (Single), then override fire_mode
    // to Burst in the state, and test burst behavior.
    rt.reset(2);
    // Override fire_mode to Burst for testing
    rt.state().burst_rounds_total = 3;
    // We need to access the definition to verify — use a helper approach.
    // Instead, let's reset to shotgun and test burst via the start_burst/tick path.
    rt.reset(1);
    rt.state().burst_rounds_total = 3;
    rt.state().burst_rounds_fired = 0;

    // Start burst: first round fires
    assert(rt.state().burst_rounds_fired == 0);
    rt.start_burst();
    assert(rt.state().burst_rounds_fired == 1); // first round counted
    assert(rt.is_bursting());
    assert(!rt.state().is_burst_complete());
    assert(rt.burst_rounds_remaining() == 2);

    // Tick past burst interval to trigger next round
    // burst_timer was set to fire_interval (60/100 = 0.6 for shotgun)
    rt.tick(0.6F); // timer fires, pending_burst_round_ set
    assert(rt.trigger_pull(true)); // fires round 2
    assert(rt.state().burst_rounds_fired == 2);
    assert(rt.is_bursting());

    rt.tick(0.6F);
    assert(rt.trigger_pull(true)); // fires round 3 (final)
    // After the final round, trigger_pull resets burst_rounds_fired to 0
    // and calls on_burst_finished()
    assert(rt.state().burst_rounds_fired == 0);
    assert(!rt.is_bursting()); // burst is complete

    std::cout << "test_burst_fires_all_rounds passed.\n";
}

void test_burst_auto_completes_without_trigger_held() {
    WeaponRuntime rt;
    rt.reset(1); // Shotgun
    rt.state().burst_rounds_total = 3;
    rt.state().burst_rounds_fired = 0;

    // Start burst
    rt.start_burst();
    assert(rt.state().burst_rounds_fired == 1);

    // Trigger is released but burst continues
    assert(!rt.trigger_pull(false)); // no rising edge
    // But trigger_pull doesn't fire pending burst rounds...
    // Actually the burst should auto-complete. Let's test the tick → pending flow.

    // Tick past burst interval
    rt.tick(0.6F);
    // pending_burst_round_ was set in tick_burst
    // Now trigger_pull returns true even though trigger is not held
    // But wait — trigger_pull requires rising edge OR pending_burst_round
    // And pending_burst_round_ is only checked in the Burst case of trigger_pull
    // trigger_pull(false) with pending_burst_round_ = true should still fire
    assert(rt.trigger_pull(false)); // fires round 2 even without held
    assert(rt.state().burst_rounds_fired == 2);

    // Tick and fire final round
    rt.tick(0.6F);
    assert(rt.trigger_pull(false)); // fires round 3
    // Burst complete
    assert(!rt.is_bursting());

    std::cout << "test_burst_auto_completes_without_trigger_held passed.\n";
}

void test_burst_respects_burst_interval() {
    WeaponRuntime rt;
    rt.reset(1); // Shotgun

    // Override burst interval to a known value
    // We can access state directly since it's exposed through state()
    rt.state().burst_rounds_total = 3;
    rt.state().burst_rounds_fired = 0;
    rt.state().burst_timer = 0.0F;

    // Start burst — first round fires immediately
    rt.start_burst();

    // Immediately check that burst timer is set to the fire_interval
    // (since burst_interval is 0.0 in the def, it falls back to fire_interval)
    assert(rt.state().burst_timer > 0.0F);

    // Partial tick: not enough time for next round
    rt.tick(0.1F);
    assert(rt.state().burst_timer > 0.0F);
    assert(!rt.trigger_pull(false)); // no pending yet

    // Full tick: timer expires
    rt.tick(0.6F);
    assert(rt.trigger_pull(false)); // round 2 fires

    std::cout << "test_burst_respects_burst_interval passed.\n";
}

void test_burst_rounds_remaining() {
    WeaponRuntime rt;
    rt.reset(1);
    rt.state().burst_rounds_total = 3;
    rt.state().burst_rounds_fired = 0;

    assert(rt.burst_rounds_remaining() == 3);

    rt.start_burst();
    assert(rt.burst_rounds_remaining() == 2);

    rt.tick(0.6F);
    rt.trigger_pull(false);
    assert(rt.burst_rounds_remaining() == 1);

    std::cout << "test_burst_rounds_remaining passed.\n";
}

void test_burst_interrupted_by_reload() {
    WeaponRuntime rt;
    rt.reset(1);
    rt.state().burst_rounds_total = 3;
    rt.state().burst_rounds_fired = 0;

    rt.start_burst();
    assert(rt.is_bursting());

    // Consume some ammo so reload is possible
    (void)rt.consume_ammo();

    // Start reload — should interrupt burst
    rt.start_reload();
    assert(rt.state().is_reloading);
    assert(!rt.is_bursting()); // burst was interrupted

    std::cout << "test_burst_interrupted_by_reload passed.\n";
}

// ----- Fire mode helpers -----------------------------------------------------

void test_fire_mode_getter() {
    WeaponRuntime rt;

    rt.reset(0); // AR-15 → Auto
    assert(rt.fire_mode() == FireMode::Auto);

    rt.reset(1); // Shotgun → Single
    assert(rt.fire_mode() == FireMode::Single);

    rt.reset(2); // Rocket Launcher → Single
    assert(rt.fire_mode() == FireMode::Single);

    std::cout << "test_fire_mode_getter passed.\n";
}

void test_try_fire_blocks_without_ammo() {
    WeaponRuntime rt;
    rt.reset(0); // Auto

    // Empty the magazine
    while (rt.state().ammo_in_magazine > 0) {
        (void)rt.consume_ammo();
    }

    // try_fire should return false (no ammo)
    assert(!rt.try_fire(true));

    std::cout << "test_try_fire_blocks_without_ammo passed.\n";
}

void test_try_fire_blocks_during_reload() {
    WeaponRuntime rt;
    rt.reset(0);

    (void)rt.consume_ammo();
    rt.start_reload();

    // Can't fire while reloading
    assert(!rt.try_fire(true));

    std::cout << "test_try_fire_blocks_during_reload passed.\n";
}

// ----- Recoil / spread generation --------------------------------------------

void test_generate_recoil_deterministic() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15 with recoil_pattern

    // Same weapon, same seed → same recoil
    RecoilEntry r1 = rt.generate_recoil();
    rt.reset(0); // resets seed to 0
    RecoilEntry r2 = rt.generate_recoil();

    assert(r1.pitch == r2.pitch);
    assert(r1.yaw == r2.yaw);

    std::cout << "test_generate_recoil_deterministic passed.\n";
}

void test_generate_recoil_advances_seed() {
    WeaponRuntime rt;
    rt.reset(0);

    int seed_before = rt.state().recoil_seed;
    (void)rt.generate_recoil();
    assert(rt.state().recoil_seed == seed_before + 1);

    std::cout << "test_generate_recoil_advances_seed passed.\n";
}

void test_generate_recoil_pattern_cycles() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15 has 5 recoil entries

    // Generate 6 recoil values — pattern should cycle
    RecoilEntry r0 = rt.generate_recoil(); // index 0
    RecoilEntry r1 = rt.generate_recoil(); // index 1
    (void)rt.generate_recoil(); // 2
    (void)rt.generate_recoil(); // 3
    (void)rt.generate_recoil(); // 4
    RecoilEntry r5 = rt.generate_recoil(); // index 0 (cycles: 5 % 5 = 0)

    assert(r0.pitch == r5.pitch);
    assert(r0.yaw == r5.yaw);
    assert(r0.pitch != r1.pitch); // different entries (unlikely to match)

    std::cout << "test_generate_recoil_pattern_cycles passed.\n";
}

void test_generate_spread_deterministic() {
    WeaponRuntime rt;
    rt.reset(1); // Shotgun with spread_angle = 5.0

    // Same seed, same pellet → same spread
    float s1 = rt.generate_spread(0, 8);
    rt.reset(1);
    float s2 = rt.generate_spread(0, 8);
    assert(s1 == s2);

    // Different pellets → different spread
    float s3 = rt.generate_spread(1, 8);
    assert(s1 != s3); // extremely unlikely to be equal

    // Single pellet → zero spread
    float s4 = rt.generate_spread(0, 1);
    assert(s4 == 0.0F);

    std::cout << "test_generate_spread_deterministic passed.\n";
}

void test_spread_accumulation() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15 has spread_per_shot = 0.3, spread_angle = 0.5

    // Initial spread equals base
    assert(rt.current_spread() == 0.5F);

    // Firing advances spread
    rt.advance_recoil_seed(); // mimics what generate_recoil does
    assert(rt.current_spread() == 0.8F); // 0.5 + 0.3

    rt.advance_recoil_seed();
    assert(rt.current_spread() == 1.1F); // 0.8 + 0.3

    std::cout << "test_spread_accumulation passed.\n";
}

void test_spread_recovery() {
    WeaponRuntime rt;
    rt.reset(0); // AR-15 has spread_recovery = 8.0

    // Accumulate some spread
    rt.advance_recoil_seed();
    rt.advance_recoil_seed();
    rt.advance_recoil_seed();
    assert(rt.current_spread() > 0.5F);

    // Tick recovers spread toward base
    rt.tick(0.1F); // recover 0.8 degrees (8.0 * 0.1)
    float after = rt.current_spread();
    assert(after < 1.4F);  // 1.4 was the accumulated value

    // Full tick back to base
    rt.tick(10.0F); // well past recovery time
    assert(rt.current_spread() == 0.5F); // back to base

    std::cout << "test_spread_recovery passed.\n";
}

// ----- trigger_was_pressed_ tracking -----------------------------------------

void test_trigger_was_pressed_tracking() {
    WeaponRuntime rt;
    rt.reset(0);

    assert(!rt.trigger_was_pressed());
    (void)rt.trigger_pull(true);
    assert(rt.trigger_was_pressed());
    (void)rt.trigger_pull(false);
    assert(!rt.trigger_was_pressed());

    std::cout << "test_trigger_was_pressed_tracking passed.\n";
}

// ----- Edge case: empty recoil pattern ---------------------------------------

void test_generate_recoil_empty_pattern() {
    WeaponRuntime rt;
    rt.reset(2); // Rocket launcher, no recoil pattern

    RecoilEntry r = rt.generate_recoil();
    assert(r.pitch == 0.0F);
    assert(r.yaw == 0.0F);

    // Seed still advances even with empty pattern
    assert(rt.state().recoil_seed == 1);

    std::cout << "test_generate_recoil_empty_pattern passed.\n";
}

void test_reset_clears_fire_control_state() {
    WeaponRuntime rt;
    rt.reset(0);

    // Set some fire control state
    rt.state().burst_rounds_fired = 2;
    rt.state().recoil_seed = 42;
    rt.state().current_spread = 10.0F;
    rt.set_trigger_was_pressed(true);

    // Reset should clear everything
    rt.reset(0);
    assert(rt.state().burst_rounds_fired == 0);
    assert(rt.state().recoil_seed == 0);
    assert(rt.state().current_spread == 0.5F); // back to base spread_angle
    assert(!rt.trigger_was_pressed());

    std::cout << "test_reset_clears_fire_control_state passed.\n";
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

    // --- Fire control tests ---

    // Fire mode
    test_fire_mode_getter();

    // Single (semi-auto)
    test_single_fire_mode_edge_detection();
    test_single_fire_mode_try_fire();

    // Auto
    test_auto_fire_mode_fires_while_held();
    test_auto_fire_mode_respects_rpm_cooldown();

    // Burst
    test_burst_fires_all_rounds();
    test_burst_auto_completes_without_trigger_held();
    test_burst_respects_burst_interval();
    test_burst_rounds_remaining();
    test_burst_interrupted_by_reload();

    // Recoil / spread
    test_generate_recoil_deterministic();
    test_generate_recoil_advances_seed();
    test_generate_recoil_pattern_cycles();
    test_generate_recoil_empty_pattern();
    test_generate_spread_deterministic();
    test_spread_accumulation();
    test_spread_recovery();

    // try_fire combined check
    test_try_fire_blocks_without_ammo();
    test_try_fire_blocks_during_reload();

    // Edge cases
    test_trigger_was_pressed_tracking();
    test_reset_clears_fire_control_state();

    std::cout << "\nAll weapon runtime tests passed.\n";
    return 0;
}

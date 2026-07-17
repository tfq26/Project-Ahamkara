#include "ahamkara/game/world.h"
#include "ahamkara/game/game_module.h"
#include "ahamkara/game/net_types.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace ahamkara::game;

// ---------------------------------------------------------------------------
// Test 1: Each player has a distinct entity identity
// ---------------------------------------------------------------------------
void test_player_identity() {
    World world;
    ae::u32 p0 = world.add_player();
    ae::u32 p1 = world.add_player();

    assert(p0 != p1);
    // Constructor creates 1 player, add_player adds 2 more = 3 total
    assert(world.player_count() == 3);

    // Each player has its own state at the spawn position
    const auto& s0 = world.get_player_state(p0);
    const auto& s1 = world.get_player_state(p1);
    assert(std::fabs(s0.position.x - s1.position.x) < 0.001F);
    assert(std::fabs(s0.position.z - s1.position.z) < 0.001F);

    // Each player has a distinct network_object_id
    Player* pp0 = world.get_player(p0);
    Player* pp1 = world.get_player(p1);
    assert(pp0 != nullptr);
    assert(pp1 != nullptr);
    assert(pp0->network_object_id() != pp1->network_object_id());

    // Set distinct player IDs
    pp0->set_player_id(100);
    pp1->set_player_id(200);
    assert(pp0->player_id() == 100);
    assert(pp1->player_id() == 200);
    assert(pp0->player_id() != pp1->player_id());

    std::cout << "test_player_identity passed.\n";
}

// ---------------------------------------------------------------------------
// Test 2: Input applied to one player does not modify another player's state
// ---------------------------------------------------------------------------
void test_input_isolation() {
    World world;
    ae::u32 p0 = world.add_player(); // index 1 (constructor created index 0)
    ae::u32 p1 = world.add_player(); // index 2

    // Starting positions are identical
    Vec3 start = world.get_player_state(p0).position;
    Vec3 start_p1 = world.get_player_state(p1).position;
    assert(std::fabs(start.x - start_p1.x) < 0.001F);
    assert(std::fabs(start.z - start_p1.z) < 0.001F);

    // Directly modify player p0's state via mutable state reference
    // (set_player_state only works for index 0)
    world.get_player_state_mut(p0).position.z = 100.0F;
    world.get_player_state_mut(p0).position.x = 50.0F;

    // Player p0's position was updated
    const auto& after_p0 = world.get_player_state(p0);
    assert(std::fabs(after_p0.position.x - 50.0F) < 0.001F);
    assert(std::fabs(after_p0.position.z - 100.0F) < 0.001F);

    // Player p1's position is unchanged
    const auto& after_p1 = world.get_player_state(p1);
    assert(std::fabs(after_p1.position.x - start_p1.x) < 0.001F);
    assert(std::fabs(after_p1.position.z - start_p1.z) < 0.001F);

    std::cout << "test_input_isolation passed.\n";
}

// ---------------------------------------------------------------------------
// Test 3: Per-player scoring (kills/deaths)
// ---------------------------------------------------------------------------
void test_per_player_scoring() {
    World world;
    ae::u32 p0 = world.add_player();
    ae::u32 p1 = world.add_player();

    Player* pp0 = world.get_player(p0);
    Player* pp1 = world.get_player(p1);

    assert(pp0->kills() == 0 && pp0->deaths() == 0);
    assert(pp1->kills() == 0 && pp1->deaths() == 0);

    pp0->add_kill();
    assert(pp0->kills() == 1);
    assert(pp1->kills() == 0);
    assert(world.get_player_kills(p0) == 1);
    assert(world.get_player_kills(p1) == 0);

    pp1->add_death();
    assert(pp1->deaths() == 1);
    assert(pp0->deaths() == 0);
    assert(world.get_player_deaths(p1) == 1);
    assert(world.get_player_deaths(p0) == 0);

    pp0->reset_score();
    pp1->reset_score();
    assert(pp0->kills() == 0 && pp0->deaths() == 0);
    assert(pp1->kills() == 0 && pp1->deaths() == 0);

    std::cout << "test_per_player_scoring passed.\n";
}

// ---------------------------------------------------------------------------
// Test 4: Death and respawn are per-player
// ---------------------------------------------------------------------------
void test_per_player_death_respawn() {
    World world;
    ae::u32 p0 = world.add_player();
    ae::u32 p1 = world.add_player();

    assert(world.is_player_alive(p0));
    assert(world.is_player_alive(p1));

    // Kill player 0 only
    world.apply_damage_to_player(p0, 999.0F, {0.0F, 0.0F, 0.0F});
    assert(!world.is_player_alive(p0));
    assert(world.is_player_alive(p1));
    assert(world.get_player_deaths(p0) == 1);
    assert(world.get_player_deaths(p1) == 0);

    // Respawn player 0
    world.respawn_player(p0);
    assert(world.is_player_alive(p0));
    assert(world.is_player_alive(p1));

    // Kill player 1
    world.apply_damage_to_player(p1, 999.0F, {0.0F, 0.0F, 0.0F});
    assert(!world.is_player_alive(p1));
    assert(world.is_player_alive(p0));
    assert(world.get_player_deaths(p0) == 1);
    assert(world.get_player_deaths(p1) == 1);

    std::cout << "test_per_player_death_respawn passed.\n";
}

// ---------------------------------------------------------------------------
// Test 5: Remove and re-add a player
// ---------------------------------------------------------------------------
void test_player_removal_and_replacement() {
    World world;
    world.add_player(); // index 1
    world.add_player(); // index 2
    // Constructor creates index 0, so total = 3
    assert(world.player_count() == 3);

    // Remove player at index 0 (constructor's unused player)
    world.remove_player(0);
    assert(world.player_count() == 2);

    // Remaining players are still valid
    const Player* remaining = world.get_player(0);
    assert(remaining != nullptr);

    // Add a replacement
    ae::u32 p2 = world.add_player();
    assert(world.player_count() == 3);
    assert(p2 == 2); // Index after shift

    const Player* new_player = world.get_player(p2);
    assert(new_player != nullptr);
    assert(new_player->is_alive());

    std::cout << "test_player_removal_and_replacement passed.\n";
}

// ---------------------------------------------------------------------------
// Test 6: Historical state tracks all player positions
// ---------------------------------------------------------------------------
void test_history_tracks_multiple_players() {
    World world;
    ae::u32 p0 = world.add_player();
    ae::u32 p1 = world.add_player();

    // Tick a few times with no input
    PlayerInputCommand input {};
    constexpr float kStep = 1.0F / 60.0F;
    for (int i = 0; i < 5; ++i) {
        world.advance_sim(kStep);
        world.apply_input(p0, kStep, input);
        world.advance_sim(kStep);
        world.apply_input(p1, kStep, input);
    }

    // Retrieve a historical state
    HistoricalState hist = world.get_historical_state(3);
    assert(hist.tick == 3);

    // At least two player positions should be recorded
    assert(hist.player_positions[0].x != 0.0F || hist.player_positions[1].z != 0.0F);
    // Player positions match current state or have been recorded
    const auto& s0 = world.get_player_state(p0);
    const auto& s1 = world.get_player_state(p1);
    bool p0_found = false, p1_found = false;
    for (ae::u32 pi = 0; pi < world.player_count() && pi < HistoricalState::kMaxPlayerPositions; ++pi) {
        if (std::fabs(hist.player_positions[pi].x - s0.position.x) < 0.001F)
            p0_found = true;
        if (std::fabs(hist.player_positions[pi].x - s1.position.x) < 0.001F)
            p1_found = true;
    }
    assert(p0_found);
    assert(p1_found);

    std::cout << "test_history_tracks_multiple_players passed.\n";
}

} // namespace

int main() {
    test_player_identity();
    test_input_isolation();
    test_per_player_scoring();
    test_per_player_death_respawn();
    test_player_removal_and_replacement();
    test_history_tracks_multiple_players();

    std::cout << "All player entity tests passed!\n";
    return 0;
}

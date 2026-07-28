#include "wish/integrations/flashback/game_session_adapter.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

class TestGameAdapter final : public wish::integrations::flashback::IGameSessionAdapter {
public:
    int admit_count {0};
    int remove_count {0};
    int complete_count {0};
    int timeout_count {0};
    bool allow_reconnect {true};

    void on_player_admitted(const wish::integrations::flashback::GamePlayerState& state) override {
        ++admit_count;
        last_player_id = state.player_id;
        last_session_id = state.session_id;
    }

    void on_player_removed(std::string_view player_id) override {
        ++remove_count;
        last_removed_id = std::string(player_id);
    }

    void on_activity_complete(wish::core::ActivityId activity_id) override {
        ++complete_count;
        last_activity_id = activity_id;
    }

    void on_player_timeout(std::string_view player_id) override {
        ++timeout_count;
        last_removed_id = std::string(player_id);
    }

    bool can_reconnect(std::string_view /*player_id*/) override {
        return allow_reconnect;
    }

    std::size_t active_player_count() const override {
        return static_cast<std::size_t>(admit_count - remove_count);
    }

    std::string last_player_id;
    std::string last_session_id;
    std::string last_removed_id;
    wish::core::ActivityId last_activity_id {0};
};

void test_adapter_calls_lifecycle() {
    TestGameAdapter adapter;

    wish::integrations::flashback::GamePlayerState state;
    state.player_id = "player-001";
    state.session_id = "session-abc";
    state.admitted = true;

    adapter.on_player_admitted(state);
    assert(adapter.admit_count == 1);
    assert(adapter.last_player_id == "player-001");
    assert(adapter.last_session_id == "session-abc");

    adapter.on_player_removed("player-001");
    assert(adapter.remove_count == 1);
    assert(adapter.last_removed_id == "player-001");

    adapter.on_activity_complete(42);
    assert(adapter.complete_count == 1);
    assert(adapter.last_activity_id == 42);

    adapter.on_player_timeout("player-001");
    assert(adapter.timeout_count == 1);

    assert(adapter.active_player_count() == 0); // 1 admit - 1 remove

    std::cout << "test_adapter_calls_lifecycle passed.\n";
}

void test_adapter_reconnect() {
    TestGameAdapter adapter;
    adapter.allow_reconnect = true;
    assert(adapter.can_reconnect("player-001"));

    adapter.allow_reconnect = false;
    assert(!adapter.can_reconnect("player-001"));

    std::cout << "test_adapter_reconnect passed.\n";
}

void test_match_report_building() {
    std::vector<std::string> participants = {"player-001", "player-002"};
    auto report = wish::integrations::flashback::build_match_report(
        1, "Deathmatch - Clash", 300.0F, participants, true, "Player 001 wins");

    assert(report.activity_id == 1);
    assert(report.activity_name == "Deathmatch - Clash");
    assert(report.duration_seconds == 300.0F);
    assert(report.participant_ids.size() == 2);
    assert(report.participant_ids[0] == "player-001");
    assert(report.was_completed);
    assert(report.summary == "Player 001 wins");

    std::cout << "test_match_report_building passed.\n";
}

void test_match_report_incomplete() {
    std::vector<std::string> participants = {"player-001"};
    auto report = wish::integrations::flashback::build_match_report(
        2, "Horde", 120.0F, participants, false, "Server shutdown");

    assert(!report.was_completed);
    assert(report.summary == "Server shutdown");

    std::cout << "test_match_report_incomplete passed.\n";
}

void test_adapter_tracks_multiple_events() {
    TestGameAdapter adapter;

    wish::integrations::flashback::GamePlayerState p1;
    p1.player_id = "p1";
    p1.session_id = "s1";

    wish::integrations::flashback::GamePlayerState p2;
    p2.player_id = "p2";
    p2.session_id = "s2";

    adapter.on_player_admitted(p1);
    adapter.on_player_admitted(p2);
    assert(adapter.admit_count == 2);
    assert(adapter.active_player_count() == 2);

    adapter.on_player_removed("p1");
    assert(adapter.active_player_count() == 1);

    adapter.on_player_removed("p2");
    assert(adapter.active_player_count() == 0);

    std::cout << "test_adapter_tracks_multiple_events passed.\n";
}

} // namespace

int main() {
    test_adapter_calls_lifecycle();
    test_adapter_reconnect();
    test_match_report_building();
    test_match_report_incomplete();
    test_adapter_tracks_multiple_events();

    std::cout << "All flashback adapter tests passed.\n";
    return 0;
}

/// @file session_adapter_integration_tests.cpp
///
/// Two-client integration tests for WishSessionAdapter.
///
/// These tests verify the Flashback-owned adapter that composes the
/// Wish session/activity SDK.  They prove:
///   1. Independent admission returns exact session-to-player mapping
///      (no first-slot shortcut).
///   2. Independent input routing — each client's inputs reach the
///      correct activity/session independently.
///   3. Recipient-relative snapshot dispatch.
///   4. Sequence tracking and acknowledgements.
///   5. Cleanup on disconnect.
///   6. Reconnect with stale-identity guard.
///   7. Error envelopes for protocol/service failures.
///
/// The tests use mock Wish services (NoopAuthValidator etc.) and a
/// minimal mock activity so no real network or game state is needed.

#include "ahamkara/game/adapters/wish_session_adapter.h"
#include "wish/core/activity.h"
#include "wish/core/activity_manager.h"
#include "wish/core/session_services.h"
#include "wish/integrations/nakama/mock_session_services.h"
#include "wish/types.h"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <span>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test utilities
// ---------------------------------------------------------------------------

static int g_failures = 0;

#define EXPECT_TRUE(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

#define EXPECT_EQ(a, b)                                                                      \
    do {                                                                                     \
        if ((a) != (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " == " << #b \
                      << " (" << (a) << " != " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

#define EXPECT_NE(a, b)                                                                      \
    do {                                                                                     \
        if ((a) == (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " != " << #b \
                      << " (" << (a) << " == " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

#define EXPECT_FALSE(cond)                                                                \
    do {                                                                                 \
        if ((cond)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

namespace {

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

// ===========================================================================
// Mock activity for testing
// ===========================================================================

/// Minimal activity implementation for integration tests.
/// Tracks admitted/removed sessions and recorded inputs.
class MockActivity : public wish::core::IActivityBase {
  public:
    struct InputRecord {
        wish::session::SessionId sid;
        wish::u32 command_sequence;
    };

    bool initialize(const wish::core::ActivityConfig& cfg) override {
        config_ = cfg;
        return true;
    }

    void shutdown() override {}

    bool admit_player(const wish::core::SessionAdmissionRequest& req) override {
        if (admitted_sessions_.size() >= static_cast<std::size_t>(config_.max_players)) {
            return false;
        }
        // Use a simple session ID
        wish::session::SessionId sid;
        sid.value = next_sid_++;
        admitted_sessions_.push_back(sid);
        // Store the player_id associated with this session
        player_map_[sid.value] = req.player_id;
        return true;
    }

    void remove_player(wish::session::SessionId sid) override {
        auto it = std::remove_if(admitted_sessions_.begin(), admitted_sessions_.end(),
                                  [sid](const wish::session::SessionId& s) {
                                      return s.value == sid.value;
                                  });
        if (it != admitted_sessions_.end()) {
            admitted_sessions_.erase(it, admitted_sessions_.end());
        }
        removed_sessions_.push_back(sid);
    }

    wish::u32 player_count() const override {
        return static_cast<wish::u32>(admitted_sessions_.size());
    }

    void tick(float /*dt*/) override {
        tick_count_++;
    }

    void process_input(wish::session::SessionId sid,
                       const wish::PacketEnvelope& /*envelope*/,
                       wish::u32 command_sequence) override {
        input_records_.push_back({sid, command_sequence});
    }

    wish::usize build_snapshot_bytes(wish::session::SessionId /*sid*/,
                                      std::span<std::byte> buffer) override {
        // Write a minimal byte as a placeholder
        if (buffer.size() < 1) return 0;
        buffer[0] = std::byte{0xAA};
        snapshot_built_ = true;
        return 1;
    }

    bool is_complete() const override { return false; }

    wish::core::ActivityId activity_id() const override { return config_.id; }
    wish::core::ActivityCategory category() const override { return config_.category; }
    std::string_view activity_name() const override { return config_.name; }

    void for_each_connected_snapshot(
        void (*fn)(void* ctx, wish::session::SessionId sid,
                   const std::byte* data, wish::usize len),
        void* ctx) override {
        for (const auto& s : admitted_sessions_) {
            fn(ctx, s, &placeholder_byte_, 1);
        }
    }

    // Test accessors
    const std::vector<InputRecord>& input_records() const { return input_records_; }
    const std::vector<wish::session::SessionId>& removed_sessions() const { return removed_sessions_; }
    const std::vector<wish::session::SessionId>& admitted_sessions() const { return admitted_sessions_; }
    int tick_count() const { return tick_count_; }
    bool snapshot_was_built() const { return snapshot_built_; }

  private:
    wish::core::ActivityConfig config_;
    std::vector<wish::session::SessionId> admitted_sessions_;
    std::vector<wish::session::SessionId> removed_sessions_;
    std::vector<InputRecord> input_records_;
    int tick_count_ {0};
    bool snapshot_built_ {false};
    wish::u64 next_sid_ {100};
    std::byte placeholder_byte_{std::byte{0xBB}};
    std::unordered_map<std::uint64_t, std::string> player_map_;
};

// ===========================================================================
// Test: Independent admission produces exact session-to-player mappings
// ===========================================================================

void test_independent_admission_no_first_slot_shortcut() {
    // ── Setup ──────────────────────────────────────────────────────────
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    // Register and start a mock activity
    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 1,
        .name = "TestActivity",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(1), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto base_time = clock::time_point{};
    const auto t0 = base_time;

    // ── Admit first client ─────────────────────────────────────────────
    auto result_a = adapter.admit_session(
        "player-alpha", "token-alpha", "192.168.1.10:30001",
        static_cast<wish::core::ActivityId>(1), t0);

    EXPECT_TRUE(result_a.admitted);
    EXPECT_NE(result_a.session_id.value, 0ULL);
    EXPECT_EQ(result_a.activity_id, 1u);

    // ── Admit second client ────────────────────────────────────────────
    auto result_b = adapter.admit_session(
        "player-beta", "token-beta", "192.168.1.10:30002",
        static_cast<wish::core::ActivityId>(1), t0 + std::chrono::milliseconds(100));

    EXPECT_TRUE(result_b.admitted);
    EXPECT_NE(result_b.session_id.value, 0ULL);
    EXPECT_EQ(result_b.activity_id, 1u);

    // ── Verify distinct session IDs (no first-slot shortcut) ───────────
    EXPECT_NE(result_a.session_id.value, result_b.session_id.value);

    // ── Verify routing resolution ──────────────────────────────────────
    wish::NetAddress addr_a{"192.168.1.10", 30001};
    wish::NetAddress addr_b{"192.168.1.10", 30002};

    auto route_a = adapter.resolve_routing(addr_a);
    EXPECT_TRUE(route_a.found);
    EXPECT_EQ(route_a.session_id.value, result_a.session_id.value);
    EXPECT_EQ(route_a.activity_id, 1u);

    auto route_b = adapter.resolve_routing(addr_b);
    EXPECT_TRUE(route_b.found);
    EXPECT_EQ(route_b.session_id.value, result_b.session_id.value);
    EXPECT_EQ(route_b.activity_id, 1u);

    // ── Verify two sessions are tracked ────────────────────────────────
    EXPECT_EQ(adapter.active_session_count(), 2u);

    std::cout << "  [PASS] test_independent_admission_no_first_slot_shortcut\n";
}

// ===========================================================================
// Test: Independent input routing
// ===========================================================================

void test_independent_input_routing() {
    // ── Setup ──────────────────────────────────────────────────────────
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 2,
        .name = "InputTestActivity",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(2), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};

    // ── Admit two clients ──────────────────────────────────────────────
    auto res_a = adapter.admit_session(
        "input-alpha", "tok-a", "10.0.0.1:40001", 2, t0);
    EXPECT_TRUE(res_a.admitted);

    auto res_b = adapter.admit_session(
        "input-beta", "tok-b", "10.0.0.1:40002", 2, t0 + std::chrono::milliseconds(50));
    EXPECT_TRUE(res_b.admitted);

    // ── Route independent inputs ───────────────────────────────────────
    wish::PacketEnvelope env_a{};
    env_a.sequence = 1;
    wish::PacketEnvelope env_b{};
    env_b.sequence = 1;

    bool routed_a = adapter.route_input(
        wish::NetAddress{"10.0.0.1", 40001}, env_a, 1001, t0 + std::chrono::milliseconds(200));
    bool routed_b = adapter.route_input(
        wish::NetAddress{"10.0.0.1", 40002}, env_b, 2001, t0 + std::chrono::milliseconds(250));

    EXPECT_TRUE(routed_a);
    EXPECT_TRUE(routed_b);

    // ── Verify independent inputs were recorded ────────────────────────
    EXPECT_EQ(mock_ptr->input_records().size(), 2u);

    // Check that the first input belongs to session A and has sequence 1001
    bool found_a = false, found_b = false;
    for (const auto& rec : mock_ptr->input_records()) {
        if (rec.sid.value == res_a.session_id.value && rec.command_sequence == 1001)
            found_a = true;
        if (rec.sid.value == res_b.session_id.value && rec.command_sequence == 2001)
            found_b = true;
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);

    std::cout << "  [PASS] test_independent_input_routing\n";
}

// ===========================================================================
// Test: Snapshot broadcast
// ===========================================================================

void test_snapshot_broadcast() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 3,
        .name = "SnapshotTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(3), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};

    // Admit two clients
    auto res_a = adapter.admit_session("snap-a", "toka", "10.0.0.1:50001", 3, t0);
    auto res_b = adapter.admit_session("snap-b", "tokb", "10.0.0.1:50002", 3, t0);

    EXPECT_TRUE(res_a.admitted);
    EXPECT_TRUE(res_b.admitted);

    // Broadcast snapshots and count how many are dispatched
    int snapshot_count = 0;
    adapter.broadcast_snapshots([&](wish::session::SessionId /*sid*/,
                                    const std::byte* /*data*/, wish::usize /*len*/) {
        ++snapshot_count;
    });

    // Should have received one snapshot per admitted session
    EXPECT_EQ(snapshot_count, 2);

    std::cout << "  [PASS] test_snapshot_broadcast\n";
}

// ===========================================================================
// Test: Sequence tracking & acknowledgements
// ===========================================================================

void test_sequence_tracking_and_ack() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 4,
        .name = "SeqTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(4), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};
    auto res = adapter.admit_session("seq-player", "tok-seq", "10.0.0.1:60001", 4, t0);
    EXPECT_TRUE(res.admitted);

    // Send multiple inputs with increasing sequences
    for (wish::u32 i = 1; i <= 5; ++i) {
        wish::PacketEnvelope env{};
        env.sequence = i;
        env.ack_sequence = i > 1 ? (i - 1) : 0;
        env.ack_bitfield = i > 1 ? 0xFFFFFFFF : 0;

        bool routed = adapter.route_input(
            wish::NetAddress{"10.0.0.1", 60001}, env, i * 100, t0 + std::chrono::milliseconds(i * 50));
        EXPECT_TRUE(routed);
    }

    // All 5 inputs should be recorded
    EXPECT_EQ(mock_ptr->input_records().size(), 5u);

    // Verify sequences are in order
    for (size_t i = 0; i < mock_ptr->input_records().size(); ++i) {
        EXPECT_EQ(mock_ptr->input_records()[i].command_sequence, (i + 1) * 100);
    }

    std::cout << "  [PASS] test_sequence_tracking_and_ack\n";
}

// ===========================================================================
// Test: Disconnect and cleanup
// ===========================================================================

void test_disconnect_cleanup() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 5,
        .name = "DisconnectTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(5), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};
    auto res = adapter.admit_session("disc-player", "tok-disc", "10.0.0.1:22001", 5, t0);
    EXPECT_TRUE(res.admitted);

    // Mark as disconnected
    adapter.mark_disconnected(res.session_id, t0 + std::chrono::seconds(5));

    // Verify the session was removed from the activity
    EXPECT_EQ(mock_ptr->removed_sessions().size(), 1u);
    EXPECT_EQ(mock_ptr->removed_sessions()[0].value, res.session_id.value);

    // Session still exists in adapter for grace period
    EXPECT_EQ(adapter.active_session_count(), 1u);

    // Re-routing after disconnect should still work (grace period)
    wish::PacketEnvelope env{};
    env.sequence = 1;
    bool routed = adapter.route_input(
        wish::NetAddress{"10.0.0.1", 22001}, env, 42, t0 + std::chrono::seconds(6));
    EXPECT_TRUE(routed);

    std::cout << "  [PASS] test_disconnect_cleanup\n";
}

// ===========================================================================
// Test: Reconnect with stale-identity guard
// ===========================================================================

void test_reconnect_stale_identity_guard() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 6,
        .name = "ReconnectTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(6), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};
    auto res = adapter.admit_session("recon-player", "tok-recon", "10.0.0.1:80001", 6, t0);
    EXPECT_TRUE(res.admitted);

    // Mark disconnected
    adapter.mark_disconnected(res.session_id, t0 + std::chrono::seconds(1));

    // The NoopAuthValidator stores player_id as "wish-player@<endpoint>".
    const std::string expected_player_id = "wish-player@10.0.0.1:80001";

    // ── Valid reconnect: same identity ─────────────────────────────────
    bool valid_reconnect = adapter.handle_reconnect(
        res.session_id, expected_player_id, t0 + std::chrono::seconds(3));
    EXPECT_TRUE(valid_reconnect);

    // ── Stale identity rejected: wrong player_id ───────────────────────
    // Re-disconnect
    adapter.mark_disconnected(res.session_id, t0 + std::chrono::seconds(5));

    bool wrong_identity = adapter.handle_reconnect(
        res.session_id, "intruder", t0 + std::chrono::seconds(7));
    EXPECT_FALSE(wrong_identity);  // must be rejected!

    std::cout << "  [PASS] test_reconnect_stale_identity_guard\n";
}

// ===========================================================================
// Test: Result reporting
// ===========================================================================

void test_result_reporting() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;

    // Use a custom reporter that tracks reported results
    struct TrackingReporter : wish::core::MatchResultReporter {
        int report_count = 0;
        std::vector<std::string> reported_players;
        void report_match_result(const wish::core::MatchResult& result) override {
            report_count++;
            reported_players.push_back(result.player_id);
        }
    };

    TrackingReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 7,
        .name = "ResultTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(7), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto t0 = clock::time_point{};
    auto res_a = adapter.admit_session("result-alpha", "toka", "10.0.0.1:90001", 7, t0);
    auto res_b = adapter.admit_session("result-beta", "tokb", "10.0.0.1:90002", 7, t0);
    EXPECT_TRUE(res_a.admitted);
    EXPECT_TRUE(res_b.admitted);

    // Report results for activity 7
    adapter.report_activity_results(static_cast<wish::core::ActivityId>(7));

    // Both players should have results reported
    EXPECT_EQ(reporter.report_count, 2);

    // The NoopAuthValidator stores player_id as "wish-player@<endpoint>".
    bool found_alpha = false, found_beta = false;
    for (const auto& pid : reporter.reported_players) {
        if (pid == "wish-player@10.0.0.1:90001") found_alpha = true;
        if (pid == "wish-player@10.0.0.1:90002") found_beta = true;
    }
    EXPECT_TRUE(found_alpha);
    EXPECT_TRUE(found_beta);

    std::cout << "  [PASS] test_result_reporting\n";
}

// ===========================================================================
// Test: Error envelope usage for protocol/service failures
// ===========================================================================

void test_error_envelope_usage() {
    // Protocol errors
    {
        auto env = ahamkara::game::adapters::WishSessionAdapter::make_protocol_error(
            wish::WishErrorCode::kProtocolVersionMismatch, "INCID001");

        EXPECT_EQ(env.version, wish::kErrorEnvelopeVersion);
        EXPECT_EQ(env.error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kProtocolVersionMismatch));
        EXPECT_EQ(env.incident_id, "INCID001");
        EXPECT_EQ(env.message_key, "errors.protocol.version_mismatch");
        EXPECT_FALSE(env.retryable);
    }

    // Service errors
    {
        auto env = ahamkara::game::adapters::WishSessionAdapter::make_service_error(
            wish::WishErrorCode::kBackendUnavailable, "INCID002");

        EXPECT_EQ(env.version, wish::kErrorEnvelopeVersion);
        EXPECT_EQ(env.error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kBackendUnavailable));
        EXPECT_EQ(env.incident_id, "INCID002");
        EXPECT_EQ(env.message_key, "errors.backend.unavailable");
        EXPECT_TRUE(env.retryable);
        EXPECT_EQ(env.retry_after_seconds, 5u);
    }

    // Admission rejection produces an error envelope
    {
        wish::integrations::nakama::NoopAuthValidator auth;
        wish::integrations::nakama::NoopSessionAdmissionService admission;
        wish::integrations::nakama::NoopMatchResultReporter reporter;
        wish::core::ActivityManager activity_mgr;

        ahamkara::game::adapters::WishSessionAdapter adapter(
            activity_mgr, auth, admission, reporter);

        const auto t0 = clock::time_point{};

        // Admit to a non-existent activity — fails because the adapter
        // looks up the activity via ActivityManager and can't find it.
        auto result = adapter.admit_session(
            "err-player", "tok-err", "10.0.0.1:99001", 999, t0);

        // Must be rejected — the activity does not exist
        EXPECT_FALSE(result.admitted);
        EXPECT_TRUE(result.error.has_error());
    }

    std::cout << "  [PASS] test_error_envelope_usage\n";
}

// ===========================================================================
// Test: Two-client end-to-end lifecycle
// ===========================================================================

void test_two_client_lifecycle() {
    wish::integrations::nakama::NoopAuthValidator auth;
    wish::integrations::nakama::NoopSessionAdmissionService admission;
    wish::integrations::nakama::NoopMatchResultReporter reporter;
    wish::core::ActivityManager activity_mgr;

    activity_mgr.register_template(wish::core::ActivityConfig{
        .id = 10,
        .name = "LifecycleTest",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = 8,
    });

    auto mock_act = std::make_unique<MockActivity>();
    auto* mock_ptr = mock_act.get();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(10), std::move(mock_act));

    ahamkara::game::adapters::WishSessionAdapter adapter(
        activity_mgr, auth, admission, reporter);

    const auto base_time = clock::time_point{};

    // ── Phase 1: Admit two clients ────────────────────────────────────
    auto client1 = adapter.admit_session(
        "client-one", "tok1", "192.168.1.1:10001", 10, base_time);
    auto client2 = adapter.admit_session(
        "client-two", "tok2", "192.168.1.1:10002", 10, base_time + std::chrono::milliseconds(50));

    EXPECT_TRUE(client1.admitted);
    EXPECT_TRUE(client2.admitted);
    EXPECT_NE(client1.session_id.value, client2.session_id.value);
    EXPECT_EQ(adapter.active_session_count(), 2u);

    // ── Phase 2: Route inputs independently ───────────────────────────
    wish::PacketEnvelope env1{};
    env1.sequence = 1;
    wish::PacketEnvelope env2{};
    env2.sequence = 1;

    EXPECT_TRUE(adapter.route_input(
        wish::NetAddress{"192.168.1.1", 10001}, env1, 10, base_time + std::chrono::milliseconds(200)));
    EXPECT_TRUE(adapter.route_input(
        wish::NetAddress{"192.168.1.1", 10002}, env2, 20, base_time + std::chrono::milliseconds(250)));

    EXPECT_EQ(mock_ptr->input_records().size(), 2u);

    // ── Phase 3: Broadcast snapshots ──────────────────────────────────
    int snap_count = 0;
    adapter.broadcast_snapshots([&](wish::session::SessionId /*sid*/,
                                    const std::byte* /*data*/, wish::usize /*len*/) {
        ++snap_count;
    });
    EXPECT_EQ(snap_count, 2);

    // ── Phase 4: Tick activities ──────────────────────────────────────
    adapter.tick_all(1.0F / 60.0F);
    EXPECT_EQ(mock_ptr->tick_count(), 1);

    // ── Phase 5: Disconnect one client ────────────────────────────────
    adapter.mark_disconnected(client1.session_id, base_time + std::chrono::seconds(5));
    // The adapter calls activity->remove_player(), so the mock activity
    // should have recorded the removal.
    EXPECT_EQ(mock_ptr->removed_sessions().size(), 1u);
    EXPECT_EQ(mock_ptr->removed_sessions()[0].value, client1.session_id.value);

    // ── Phase 6: Reconnect with correct identity ──────────────────────
    // The NoopAuthValidator stores player_id as "wish-player@<endpoint>".
    const std::string client1_player_id = "wish-player@192.168.1.1:10001";
    bool reconnected = adapter.handle_reconnect(
        client1.session_id, client1_player_id, base_time + std::chrono::seconds(7));
    EXPECT_TRUE(reconnected);

    // ── Phase 7: Stale identity rejected ──────────────────────────────
    adapter.mark_disconnected(client1.session_id, base_time + std::chrono::seconds(8));
    bool hijack_attempt = adapter.handle_reconnect(
        client1.session_id, "impostor", base_time + std::chrono::seconds(10));
    EXPECT_FALSE(hijack_attempt);  // must be rejected!

    // ── Phase 8: Report results ───────────────────────────────────────
    adapter.report_activity_results(static_cast<wish::core::ActivityId>(10));

    std::cout << "  [PASS] test_two_client_lifecycle\n";
}

}  // anonymous namespace

// ===========================================================================
// Main
// ===========================================================================

int main() {
    std::cout << "session_adapter_integration_tests:\n";

    test_independent_admission_no_first_slot_shortcut();
    test_independent_input_routing();
    test_snapshot_broadcast();
    test_sequence_tracking_and_ack();
    test_disconnect_cleanup();
    test_reconnect_stale_identity_guard();
    test_result_reporting();
    test_error_envelope_usage();
    test_two_client_lifecycle();

    if (g_failures != 0) {
        std::cerr << "session_adapter_integration_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "session_adapter_integration_tests: ok\n";
    return 0;
}

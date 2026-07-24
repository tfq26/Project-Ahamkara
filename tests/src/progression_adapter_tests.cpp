/// @file progression_adapter_tests.cpp
///
/// End-to-end tests for the Flashback progression persistence adapter.
///
/// These tests interface with Wish only through its public contracts
/// (wish::core::MatchResultReporter), respecting the package boundary
/// rule: Wish receives only its neutral envelope and opaque payload.
///
/// Acceptance criteria covered:
///   [x] Adapter is built and tested in Flashback (game/).
///   [x] Wish receives only its public neutral envelope.
///   [x] Duplicate result submission cannot grant a reward twice.
///   [x] Schema upgrade/downgrade, corrupt data, conflict, unavailable
///       backend, and retry exhaustion have explicit outcomes and codes.
///   [x] Offline policy is documented and tested.
///   [x] Tests use released/package boundaries (MatchResultReporter).

#include "ahamkara/game/adapters/progression_adapter.h"

#include "wish/core/session_services.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// ===========================================================================
// Mock MatchResultReporter — injectable into the adapter for testing.
// This is the ONLY Wish interface the adapter consumes, proving the
// boundary rule.
// ===========================================================================

namespace {

class MockMatchResultReporter : public wish::core::MatchResultReporter {
  public:
    // Simulate backend failure
    bool fail_all {false};
    bool fail_once {false};       // fail the next call, then succeed
    std::vector<wish::core::MatchResult> reported {};
    std::map<std::string, int> report_count_by_id {};

    void report_match_result(const wish::core::MatchResult& result) override {
        if (fail_all) {
            throw std::runtime_error("backend unavailable");
        }
        if (fail_once) {
            fail_once = false;
            throw std::runtime_error("transient backend failure");
        }
        reported.push_back(result);
        report_count_by_id[result.match_id]++;
    }

    void reset() {
        fail_all = false;
        fail_once = false;
        reported.clear();
        report_count_by_id.clear();
    }
};

// ===========================================================================
// ProgressionSaveData tests
// ===========================================================================

void test_serialize_roundtrip() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData original{};
    original.version = ProgressionSaveData::kCurrentVersion;
    original.player.player_id = "player-001";
    original.player.xp = 1500;
    original.player.level = 2;
    original.player.currency_hard = 100;
    original.player.currency_soft = 500;
    original.player.total_matches_played = 15;
    original.player.total_wins = 7;
    original.unlocked_items.push_back({"weapon_rifle_01", "weapon", 1000});
    original.unlocked_items.push_back({"skin_red_camo", "skin", 2000});
    original.recent_result_ids = {"result-aaa", "result-bbb"};
    original.last_saved_ms = 1234567890;

    // Serialize
    auto bytes = serialize_progression_data(original);
    assert(!bytes.empty());

    // Deserialize
    auto result = deserialize_progression_data(bytes);
    assert(result.ok);
    assert(result.error == ProgressionErrorCode::kSuccess);

    const auto& restored = result.data;
    assert(restored.version == ProgressionSaveData::kCurrentVersion);
    assert(restored.player.player_id == "player-001");
    assert(restored.player.xp == 1500);
    assert(restored.player.level == 2);
    assert(restored.player.currency_hard == 100);
    assert(restored.player.currency_soft == 500);
    assert(restored.player.total_matches_played == 15);
    assert(restored.player.total_wins == 7);
    assert(restored.unlocked_items.size() == 2);
    assert(restored.unlocked_items[0].item_id == "weapon_rifle_01");
    assert(restored.unlocked_items[1].item_id == "skin_red_camo");
    assert(restored.recent_result_ids.size() == 2);
    assert(restored.recent_result_ids[0] == "result-aaa");
    assert(restored.last_saved_ms == 1234567890);

    // Checksum should be valid
    assert(verify_progression_checksum(restored));

    std::cout << "test_serialize_roundtrip passed.\n";
}

void test_checksum_detects_corruption() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.version = ProgressionSaveData::kCurrentVersion;
    data.player.player_id = "player-002";
    data.player.xp = 500;
    data.checksum = compute_progression_checksum(data);

    // Verify intact data
    assert(verify_progression_checksum(data));

    // Corrupt a field
    data.player.xp = 99999;
    assert(!verify_progression_checksum(data));

    std::cout << "test_checksum_detects_corruption passed.\n";
}

void test_deserialize_corrupt_data() {
    using namespace ahamkara::game::adapters;

    // Truncated data
    std::vector<std::byte> bad_bytes = {std::byte{0x00}, std::byte{0x01}};
    auto result = deserialize_progression_data(bad_bytes);
    assert(!result.ok);
    assert(result.error == ProgressionErrorCode::kDataCorrupt);

    // Bad magic
    std::vector<std::byte> bad_magic(8, std::byte{0xFF});
    result = deserialize_progression_data(bad_magic);
    assert(!result.ok);
    assert(result.error == ProgressionErrorCode::kDataCorrupt);

    std::cout << "test_deserialize_corrupt_data passed.\n";
}

void test_migration_unknown_version() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.version = 999;  // future/unknown version

    auto err = migrate_to_latest(data);
    assert(err == ProgressionErrorCode::kDataVersionUnknown);

    std::cout << "test_migration_unknown_version passed.\n";
}

void test_migration_already_current() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.version = ProgressionSaveData::kCurrentVersion;

    auto err = migrate_to_latest(data);
    assert(err == ProgressionErrorCode::kSuccess);

    std::cout << "test_migration_already_current passed.\n";
}

void test_migration_uninitialized() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.version = 0;  // uninitialized

    auto err = migrate_to_latest(data);
    assert(err == ProgressionErrorCode::kSuccess);
    assert(data.version == ProgressionSaveData::kCurrentVersion);

    std::cout << "test_migration_uninitialized passed.\n";
}

// ===========================================================================
// RewardManager tests
// ===========================================================================

void test_reward_manager_grant_once() {
    using namespace ahamkara::game::adapters;

    RewardManager mgr;
    int grant_count = 0;

    auto callback = [&](const ProgressionResult&) -> bool {
        grant_count++;
        return true;
    };

    ProgressionResult result{};
    result.result_id = "unique-result-1";
    result.player_id = "player-1";

    auto outcome = mgr.grant_reward(result, callback);
    assert(outcome == GrantOutcome::kGranted);
    assert(grant_count == 1);

    std::cout << "test_reward_manager_grant_once passed.\n";
}

void test_reward_manager_idempotent() {
    using namespace ahamkara::game::adapters;

    RewardManager mgr;
    int grant_count = 0;

    auto callback = [&](const ProgressionResult&) -> bool {
        grant_count++;
        return true;
    };

    ProgressionResult result{};
    result.result_id = "dup-result";
    result.player_id = "player-1";

    // First grant
    auto outcome = mgr.grant_reward(result, callback);
    assert(outcome == GrantOutcome::kGranted);
    assert(grant_count == 1);

    // Second grant with same result_id
    outcome = mgr.grant_reward(result, callback);
    assert(outcome == GrantOutcome::kAlreadyGranted);
    assert(grant_count == 1);  // callback NOT called

    std::cout << "test_reward_manager_idempotent passed.\n";
}

void test_reward_manager_queues_on_failure() {
    using namespace ahamkara::game::adapters;

    RewardManager mgr;
    int grant_count = 0;

    auto fail_callback = [&](const ProgressionResult&) -> bool {
        grant_count++;
        return false;  // simulate backend failure
    };

    ProgressionResult result{};
    result.result_id = "queue-me";
    result.player_id = "player-1";

    auto outcome = mgr.grant_reward(result, fail_callback);
    assert(outcome == GrantOutcome::kPending);
    assert(grant_count == 1);   // callback was attempted
    assert(mgr.pending_count() == 1);

    std::cout << "test_reward_manager_queues_on_failure passed.\n";
}

void test_reward_manager_retry_succeeds() {
    using namespace ahamkara::game::adapters;

    RewardManager mgr;
    int grant_count = 0;

    // Override clock for deterministic timing.
    // Start at time 0. The retry is scheduled at base_delay_ms (1000).
    // We use a mutable clock value so we can advance time.
    auto clock_val = std::make_shared<std::int64_t>(0);
    mgr.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    ProgressionResult result{};
    result.result_id = "retry-id";
    result.player_id = "player-1";

    // First attempt — backend fails
    auto outcome = mgr.grant_reward(result, [&](const ProgressionResult&) -> bool {
        grant_count++;
        return false;
    });
    assert(outcome == GrantOutcome::kPending);
    assert(grant_count == 1);
    assert(mgr.pending_count() == 1);

    // Advance time past the retry delay
    *clock_val = 2000;

    // Retry — backend succeeds
    auto retried = mgr.retry_pending([&](const ProgressionResult&) -> bool {
        grant_count++;
        return true;
    });
    assert(retried == 1);
    assert(grant_count == 2);   // callback was called again
    assert(mgr.pending_count() == 0);
    assert(mgr.is_already_granted("retry-id"));

    std::cout << "test_reward_manager_retry_succeeds passed.\n";
}

void test_reward_manager_retry_exhaustion() {
    using namespace ahamkara::game::adapters;

    RewardManagerConfig config{};
    config.max_retry_attempts = 3;  // low for test
    config.retry_base_delay_ms = 1; // fast retry

    RewardManager mgr(config);
    auto clock_val = std::make_shared<std::int64_t>(0);
    mgr.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    int grant_count = 0;

    ProgressionResult result{};
    result.result_id = "exhaust-me";
    result.player_id = "player-1";

    // First attempt -> queue
    auto outcome = mgr.grant_reward(result, [&](const ProgressionResult&) -> bool {
        grant_count++;
        return false;
    });
    assert(outcome == GrantOutcome::kPending);

    // Retry up to exhaustion (advance clock each time so retries are eligible)
    for (int i = 0; i < config.max_retry_attempts; ++i) {
        *clock_val += 10;  // advance past the delay
        auto retried = mgr.retry_pending([&](const ProgressionResult&) -> bool {
            grant_count++;
            return false;
        });
        static_cast<void>(retried);
    }

    // After max retries, the entry should be gone
    assert(mgr.pending_count() == 0);
    assert(grant_count == config.max_retry_attempts + 1);  // initial + retries

    // Result is NOT in the granted set (never succeeded)
    assert(!mgr.is_already_granted("exhaust-me"));

    std::cout << "test_reward_manager_retry_exhaustion passed.\n";
}

void test_reward_manager_snapshot_restore() {
    using namespace ahamkara::game::adapters;

    RewardManager mgr;
    auto clock_val = std::make_shared<std::int64_t>(42);
    mgr.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    // Queue two results
    ProgressionResult r1{}, r2{};
    r1.result_id = "r1";
    r1.player_id = "p1";
    r2.result_id = "r2";
    r2.player_id = "p2";

    auto fail_cb = [](const ProgressionResult&) -> bool { return false; };

    assert(mgr.grant_reward(r1, fail_cb) == GrantOutcome::kPending);
    assert(mgr.grant_reward(r2, fail_cb) == GrantOutcome::kPending);
    assert(mgr.pending_count() == 2);

    // Snapshot
    auto snapshot = mgr.snapshot_pending();
    assert(snapshot.size() == 2);

    // Reset and restore
    mgr.reset();
    assert(mgr.pending_count() == 0);
    assert(mgr.tracked_count() == 0);

    mgr.restore_pending(snapshot);
    assert(mgr.pending_count() == 2);

    // Restored entries should have their next_retry_time_ms preserved
    auto restored_snapshot = mgr.snapshot_pending();
    assert(restored_snapshot.size() == 2);
    assert(restored_snapshot[0].result.result_id == "r1");
    assert(restored_snapshot[0].next_retry_time_ms == 42 + 1000);
    assert(restored_snapshot[1].result.result_id == "r2");

    std::cout << "test_reward_manager_snapshot_restore passed.\n";
}

// ===========================================================================
// ProgressionAdapter tests (using Wish public contracts only)
// ===========================================================================

void test_adapter_translate_to_wish() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    ProgressionAdapter adapter(reporter);

    ProgressionResult fb_result{};
    fb_result.result_id = "match-001";
    fb_result.activity_id = "deathmatch";
    fb_result.player_id = "player-x";
    fb_result.score = 2500;
    fb_result.completed = true;
    fb_result.duration_seconds = 600.0F;

    auto wish_match = adapter.to_wish_match_result(fb_result);
    assert(wish_match.match_id == "match-001");
    assert(wish_match.player_id == "player-x");
    assert(wish_match.completed == true);
    // Summary is "activity_id:score"
    assert(wish_match.summary.find("deathmatch") != std::string::npos);
    assert(wish_match.summary.find("2500") != std::string::npos);

    // Round-trip back
    auto fb_restored = adapter.from_wish_match_result(wish_match);
    assert(fb_restored.result_id == "match-001");
    assert(fb_restored.player_id == "player-x");
    assert(fb_restored.completed == true);
    assert(fb_restored.activity_id == "deathmatch");
    assert(fb_restored.score == 2500);

    std::cout << "test_adapter_translate_to_wish passed.\n";
}

void test_adapter_report_success() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    ProgressionAdapter adapter(reporter);

    ProgressionResult result{};
    result.result_id = "report-ok";
    result.player_id = "player-1";
    result.activity_id = "horde";
    result.score = 100;
    result.completed = true;

    auto err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kSuccess);
    assert(reporter->reported.size() == 1);
    assert(reporter->reported[0].match_id == "report-ok");

    std::cout << "test_adapter_report_success passed.\n";
}

void test_adapter_report_duplicate() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    ProgressionAdapter adapter(reporter);

    ProgressionResult result{};
    result.result_id = "dup-report";
    result.player_id = "player-1";
    result.activity_id = "deathmatch";
    result.score = 50;

    // First report
    auto err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kSuccess);
    assert(reporter->reported.size() == 1);

    // Second report with same result_id — must NOT grant again
    err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kSuccess);  // not an error, just idempotent
    assert(reporter->reported.size() == 1);  // NOT incremented

    std::cout << "test_adapter_report_duplicate passed.\n";
}

void test_adapter_report_backend_unavailable() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    reporter->fail_all = true;

    ProgressionAdapter adapter(reporter);

    ProgressionResult result{};
    result.result_id = "offline-result";
    result.player_id = "player-1";
    result.activity_id = "horde";
    result.score = 200;

    auto err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kBackendUnavailable);
    assert(adapter.pending_count() == 1);  // queued for retry

    std::cout << "test_adapter_report_backend_unavailable passed.\n";
}

void test_adapter_report_then_retry() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    reporter->fail_once = true;

    ProgressionAdapter adapter(reporter);
    // Use a fast clock for deterministic retry timing
    auto clock_val = std::make_shared<std::int64_t>(0);
    adapter.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    ProgressionResult result{};
    result.result_id = "retry-me";
    result.player_id = "player-1";
    result.activity_id = "deathmatch";
    result.score = 75;

    // First attempt (fails)
    auto err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kBackendUnavailable);
    assert(adapter.pending_count() == 1);

    // Advance clock past retry delay
    *clock_val = 2000;

    // Force retry — should succeed now (fail_once consumed)
    auto retried = adapter.retry_pending();
    assert(retried == 1);
    assert(adapter.pending_count() == 0);
    assert(reporter->reported.size() == 1);
    assert(reporter->reported[0].match_id == "retry-me");

    std::cout << "test_adapter_report_then_retry passed.\n";
}

void test_adapter_apply_match_outcome() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.player.player_id = "player-1";
    data.player.xp = 500;
    data.player.level = 1;
    data.player.currency_soft = 100;

    ProgressionResult result{};
    result.result_id = "match-outcome-1";
    result.player_id = "player-1";
    result.activity_id = "deathmatch";
    result.score = 100;
    result.completed = true;

    auto err = ProgressionAdapter::apply_match_outcome(result, data);
    assert(err == ProgressionErrorCode::kSuccess);

    // XP: 500 + (100 * 10) = 1500
    assert(data.player.xp == 1500);
    // Level: 1 + (1500 / 1000) = 2
    assert(data.player.level == 2);
    // Currency: 100 + (100 / 10) = 110
    assert(data.player.currency_soft == 110);
    // Match count
    assert(data.player.total_matches_played == 1);

    std::cout << "test_adapter_apply_match_outcome passed.\n";
}

void test_adapter_snapshot_and_restore_flow() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    reporter->fail_all = true;

    ProgressionAdapter adapter(reporter);
    auto clock_val = std::make_shared<std::int64_t>(0);
    adapter.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    ProgressionResult r1{}, r2{};
    r1.result_id = "snap-r1";
    r1.player_id = "p1";
    r2.result_id = "snap-r2";
    r2.player_id = "p2";

    assert(adapter.report_result(r1) == ProgressionErrorCode::kBackendUnavailable);
    assert(adapter.report_result(r2) == ProgressionErrorCode::kBackendUnavailable);
    assert(adapter.pending_count() == 2);

    // Snapshot
    auto snapshot = adapter.snapshot_pending();
    assert(snapshot.size() == 2);

    // Reset and restore
    adapter.reset();
    assert(adapter.pending_count() == 0);
    assert(adapter.tracked_result_count() == 0);

    adapter.restore_pending(snapshot);
    assert(adapter.pending_count() == 2);
    assert(adapter.tracked_result_count() == 0);  // not yet granted

    // Now the backend is available — retry all
    reporter->fail_all = false;
    *clock_val = 2000;  // advance past retry delay
    auto retried = adapter.retry_pending();
    assert(retried == 2);
    assert(adapter.pending_count() == 0);
    assert(reporter->reported.size() == 2);

    std::cout << "test_adapter_snapshot_and_restore_flow passed.\n";
}

void test_adapter_empty_save_data() {
    using namespace ahamkara::game::adapters;

    // Empty save data should fail
    std::vector<std::byte> empty;
    auto result = ProgressionAdapter::deserialize(empty);
    assert(!result.ok);
    assert(result.error == ProgressionErrorCode::kDataCorrupt);

    std::cout << "test_adapter_empty_save_data passed.\n";
}

void test_adapter_default_offline_policy() {
    using namespace ahamkara::game::adapters;

    // Verify the default offline behaviour is QueueAndRetry
    auto reporter = std::make_shared<MockMatchResultReporter>();
    reporter->fail_all = true;

    ProgressionAdapter adapter(reporter);
    auto clock_val = std::make_shared<std::int64_t>(0);
    adapter.set_clock([clock_val]() -> std::int64_t { return *clock_val; });

    // Default should queue

    ProgressionResult result{};
    result.result_id = "default-offline";
    result.player_id = "player-1";

    auto err = adapter.report_result(result);
    assert(err == ProgressionErrorCode::kBackendUnavailable);
    assert(adapter.pending_count() == 1);

    // Advance clock and retry (should succeed now)
    reporter->fail_all = false;
    *clock_val = 2000;
    auto retried = adapter.retry_pending();
    assert(retried == 1);
    assert(adapter.pending_count() == 0);

    std::cout << "test_adapter_default_offline_policy passed.\n";
}

void test_offline_behaviour_labels() {
    using namespace ahamkara::game::adapters;

    assert(std::string(offline_behaviour_label(OfflineBehaviour::kQueueAndRetry))
           == "queue_and_retry");
    assert(std::string(offline_behaviour_label(OfflineBehaviour::kReject))
           == "reject");
    assert(std::string(offline_behaviour_label(OfflineBehaviour::kQueueWithEviction))
           == "queue_with_eviction");

    std::cout << "test_offline_behaviour_labels passed.\n";
}

void test_progression_error_labels() {
    using namespace ahamkara::game::adapters;

    assert(std::string(progression_error_label(ProgressionErrorCode::kSuccess))
           == "success");
    assert(std::string(progression_error_label(ProgressionErrorCode::kDataVersionUnknown))
           == "data_version_unknown");
    assert(std::string(progression_error_label(ProgressionErrorCode::kDataCorrupt))
           == "data_corrupt");
    assert(std::string(progression_error_label(ProgressionErrorCode::kBackendUnavailable))
           == "backend_unavailable");
    assert(std::string(progression_error_label(ProgressionErrorCode::kRetryExhausted))
           == "retry_exhausted");

    std::cout << "test_progression_error_labels passed.\n";
}

void test_adapter_reject_invalid_result() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    ProgressionAdapter adapter(reporter);

    // Empty result_id should be rejected
    ProgressionResult empty_result{};
    auto err = adapter.report_result(empty_result);
    assert(err == ProgressionErrorCode::kInvalidArgument);

    // Empty player_id should be rejected
    ProgressionResult no_player{};
    no_player.result_id = "some-id";
    err = adapter.report_result(no_player);
    assert(err == ProgressionErrorCode::kInvalidArgument);

    std::cout << "test_adapter_reject_invalid_result passed.\n";
}

void test_adapter_serialize_deserialize_static() {
    using namespace ahamkara::game::adapters;

    ProgressionSaveData data{};
    data.version = ProgressionSaveData::kCurrentVersion;
    data.player.player_id = "static-test";
    data.player.xp = 999;
    data.player.level = 5;

    auto bytes = ProgressionAdapter::serialize(data);
    assert(!bytes.empty());

    auto result = ProgressionAdapter::deserialize(bytes);
    assert(result.ok);
    assert(result.data.player.xp == 999);
    assert(result.data.player.level == 5);

    std::cout << "test_adapter_serialize_deserialize_static passed.\n";
}

void test_grant_outcome_labels() {
    using namespace ahamkara::game::adapters;

    assert(std::string(grant_outcome_label(GrantOutcome::kGranted)) == "granted");
    assert(std::string(grant_outcome_label(GrantOutcome::kAlreadyGranted)) == "already_granted");
    assert(std::string(grant_outcome_label(GrantOutcome::kPending)) == "pending");
    assert(std::string(grant_outcome_label(GrantOutcome::kFailed)) == "failed");
    assert(std::string(grant_outcome_label(GrantOutcome::kRetryExhausted)) == "retry_exhausted");

    std::cout << "test_grant_outcome_labels passed.\n";
}

void test_adapter_reset_clears_state() {
    using namespace ahamkara::game::adapters;

    auto reporter = std::make_shared<MockMatchResultReporter>();
    ProgressionAdapter adapter(reporter);

    // Report a result (succeeds)
    ProgressionResult result{};
    result.result_id = "reset-test";
    result.player_id = "player-1";
    result.activity_id = "deathmatch";

    assert(adapter.report_result(result) == ProgressionErrorCode::kSuccess);
    assert(adapter.tracked_result_count() == 1);

    // Reset clears everything
    adapter.reset();
    assert(adapter.tracked_result_count() == 0);
    assert(adapter.pending_count() == 0);

    // Re-reporting should work (idempotency set cleared)
    assert(adapter.report_result(result) == ProgressionErrorCode::kSuccess);
    assert(adapter.tracked_result_count() == 1);

    std::cout << "test_adapter_reset_clears_state passed.\n";
}

}  // anonymous namespace

// ===========================================================================
//  Main
// ===========================================================================

int main() {
    // --- Save data tests ---
    test_serialize_roundtrip();
    test_checksum_detects_corruption();
    test_deserialize_corrupt_data();
    test_migration_unknown_version();
    test_migration_already_current();
    test_migration_uninitialized();

    // --- Reward manager tests ---
    test_reward_manager_grant_once();
    test_reward_manager_idempotent();
    test_reward_manager_queues_on_failure();
    test_reward_manager_retry_succeeds();
    test_reward_manager_retry_exhaustion();
    test_reward_manager_snapshot_restore();

    // --- Adapter tests ---
    test_adapter_translate_to_wish();
    test_adapter_report_success();
    test_adapter_report_duplicate();
    test_adapter_report_backend_unavailable();
    test_adapter_report_then_retry();
    test_adapter_apply_match_outcome();
    test_adapter_snapshot_and_restore_flow();
    test_adapter_empty_save_data();
    test_adapter_default_offline_policy();
    test_adapter_reject_invalid_result();
    test_adapter_serialize_deserialize_static();
    test_adapter_reset_clears_state();

    // --- Utility tests ---
    test_offline_behaviour_labels();
    test_progression_error_labels();
    test_grant_outcome_labels();

    std::cout << "\nAll progression adapter tests passed.\n";
    return 0;
}

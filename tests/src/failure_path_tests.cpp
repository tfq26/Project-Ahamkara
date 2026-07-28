#include "wish/core/error_codes.h"
#include "wish/core/error_catalog.h"
#include "wish/core/error_envelope.h"
#include "wish/core/session_services.h"
#include "wish/core/support_bundle.h"
#include "wish/core/observability.h"
#include "wish/core/version_compatibility.h"
#include "wish/integrations/flashback/game_session_adapter.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Custom assertion macro with counting
// ---------------------------------------------------------------------------
static int g_failure_count = 0;

#define FAILURE_TEST(cond, msg)                                                    \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cerr << "FAILURE_PATH_FAIL: " << msg << " (" << __FILE__          \
                      << ":" << __LINE__ << ")\n";                                 \
            ++g_failure_count;                                                     \
        }                                                                          \
    } while (0)

#define EXPECT_EQ(a, b) FAILURE_TEST((a) == (b), "Expected equality: " #a " == " #b)
#define EXPECT_TRUE(cond) FAILURE_TEST(cond, "Expected true: " #cond)
#define EXPECT_FALSE(cond) FAILURE_TEST(!(cond), "Expected false: " #cond)

// =========================================================================
// Identity — failure-path tests
// =========================================================================
namespace {

void test_auth_rejection_failure_path() {
    // Simulate auth rejection: validator returns rejected result
    struct RejectingValidator final : public wish::core::AuthValidator {
        wish::core::AuthResult validate(const wish::core::AuthRequest& /*request*/) const override {
            wish::core::AuthResult result {};
            result.accepted = false;
            result.error_message = "Authentication rejected: invalid token";
            return result;
        }
    };

    RejectingValidator validator;
    wish::core::AuthRequest request;
    request.token = "invalid-token";
    request.remote_endpoint = "127.0.0.1:7777";

    const auto result = validator.validate(request);
    EXPECT_FALSE(result.accepted);
    EXPECT_TRUE(result.error_message.find("rejected") != std::string::npos);
    EXPECT_TRUE(result.player_id.empty());

    // Verify the error code mapping
    const auto wish_code = wish::map_native_to_wish_code("nakama", 3);
    EXPECT_EQ(wish_code, wish::WishErrorCode::kAuthRejected);

    // Verify the catalog entry for auth rejection
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kAuthRejected);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryNow));

    std::cout << "  PASS: test_auth_rejection_failure_path\n";
}

void test_auth_empty_token_failure_path() {
    // Simulate auth with empty token
    struct StrictValidator final : public wish::core::AuthValidator {
        wish::core::AuthResult validate(const wish::core::AuthRequest& request) const override {
            if (request.token.empty()) {
                wish::core::AuthResult result {};
                result.accepted = false;
                result.error_message = "Authentication rejected: missing token";
                return result;
            }
            wish::core::AuthResult result {};
            result.accepted = true;
            result.player_id = "player-001";
            return result;
        }
    };

    StrictValidator validator;

    // Empty token should fail
    wish::core::AuthRequest empty_token;
    empty_token.token = "";
    auto result = validator.validate(empty_token);
    EXPECT_FALSE(result.accepted);

    // Valid token should pass
    wish::core::AuthRequest valid_token;
    valid_token.token = "valid-session-token";
    result = validator.validate(valid_token);
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(result.player_id, "player-001");

    std::cout << "  PASS: test_auth_empty_token_failure_path\n";
}

// =========================================================================
// Authentication — failure-path tests
// =========================================================================

void test_session_admission_rejection_failure_path() {
    // Simulate session admission rejection
    struct RejectingAdmission final : public wish::core::SessionAdmissionService {
        wish::core::SessionAdmissionResult admit(
            const wish::core::SessionAdmissionRequest& /*request*/) const override {
            wish::core::SessionAdmissionResult result {};
            result.admitted = false;
            result.error_message = "Session admission rejected: at capacity";
            return result;
        }
    };

    RejectingAdmission admission;
    wish::core::SessionAdmissionRequest request;
    request.player_id = "player-001";
    request.session_id = "session-001";
    request.remote_endpoint = "127.0.0.1:7777";

    const auto result = admission.admit(request);
    EXPECT_FALSE(result.admitted);
    EXPECT_TRUE(result.match_id.empty());

    // Verify the catalog entry
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kSessionAdmissionRejected);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);

    std::cout << "  PASS: test_session_admission_rejection_failure_path\n";
}

// =========================================================================
// Capacity — failure-path tests
// =========================================================================

void test_capacity_exceeded_failure_path() {
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kCapacityExceeded);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryBackoff));

    // Verify code domain
    EXPECT_TRUE(std::string_view(entry->domain) == "CAP");

    // Verify the WS-* code format
    char buffer[12];
    wish::format_wish_code(wish::WishErrorCode::kCapacityExceeded, buffer, sizeof(buffer));
    EXPECT_TRUE(std::string_view(buffer) == "WS-CAP-3001");

    std::cout << "  PASS: test_capacity_exceeded_failure_path\n";
}

// =========================================================================
// Protocol version mismatch — failure-path tests
// =========================================================================

void test_protocol_version_mismatch_failure_path() {
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kProtocolVersionMismatch);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RestartSession));

    char buffer[12];
    wish::format_wish_code(wish::WishErrorCode::kProtocolVersionMismatch, buffer, sizeof(buffer));
    EXPECT_TRUE(std::string_view(buffer) == "WS-PRO-4002");

    // Test version compatibility detection
    wish::core::VersionCompatibilityTable table;

    wish::core::VersionCompatibilityEntry v1;
    v1.version_pattern = "1.0.*";
    v1.min_compatible_version = "1.0.0";
    v1.description = "v1.0.x stable protocol";
    table.add_entry(v1);

    // Compatible versions
    auto report = table.check_compatibility("1.0.0", "1.0.5");
    EXPECT_TRUE(report.compatible);

    // Incompatible - major version mismatch
    report = table.check_compatibility("1.0.0", "0.9.0");
    EXPECT_FALSE(report.compatible);
    EXPECT_TRUE(report.severity == wish::core::VersionSkewSeverity::MajorSkew);
    EXPECT_FALSE(report.explanation.empty());
    EXPECT_FALSE(report.resolution_hint.empty());

    // Verify actionable diagnostic
    auto diagnostic = wish::core::format_version_report(report);
    EXPECT_FALSE(diagnostic.empty());
    EXPECT_TRUE(diagnostic.find("server=1.0.0") != std::string::npos);
    EXPECT_TRUE(diagnostic.find("client=0.9.0") != std::string::npos);
    EXPECT_TRUE(diagnostic.find("compatible=no") != std::string::npos);

    std::cout << "  PASS: test_protocol_version_mismatch_failure_path\n";
}

// =========================================================================
// Backend unavailable/timeout — failure-path tests
// =========================================================================

void test_backend_unavailable_failure_path() {
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kBackendUnavailable);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryBackoff));

    char buffer[12];
    wish::format_wish_code(wish::WishErrorCode::kBackendUnavailable, buffer, sizeof(buffer));
    EXPECT_TRUE(std::string_view(buffer) == "WS-BAK-5001");

    // Test native error mapping
    auto code = wish::map_native_to_wish_code("nakama", 1);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendUnavailable);

    code = wish::map_native_to_wish_code("nakama_grpc", 14);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendUnavailable);

    code = wish::map_native_to_wish_code("system", 60);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendUnavailable);

    // Verify retry envelope works for backend failures
    wish::ErrorEnvelope envelope;
    envelope.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kBackendUnavailable);
    envelope.incident_id = "BACKEND1";
    envelope.message_key = "errors.backend.unavailable";
    envelope.retryable = true;
    envelope.retry_after_seconds = 30;

    EXPECT_TRUE(envelope.valid());
    EXPECT_TRUE(envelope.has_error());
    EXPECT_TRUE(envelope.retryable);
    EXPECT_EQ(envelope.retry_after_seconds, 30u);

    auto serialized = wish::serialize_envelope(envelope);
    auto deserialized = wish::deserialize_envelope(serialized);
    EXPECT_TRUE(deserialized.has_value());
    EXPECT_TRUE(deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 30u);

    std::cout << "  PASS: test_backend_unavailable_failure_path\n";
}

void test_backend_timeout_failure_path() {
    const auto* entry = wish::ErrorCatalog::instance().find(wish::WishErrorCode::kBackendTimeout);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_TRUE(entry->user_visible);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryNow));

    // Test native error mapping for timeout
    auto code = wish::map_native_to_wish_code("nakama_grpc", 4);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendTimeout);

    std::cout << "  PASS: test_backend_timeout_failure_path\n";
}

// =========================================================================
// Retry and recovery — failure-path tests
// =========================================================================

void test_retry_behavior_failure_path() {
    wish::core::OperationHistory history;

    // Simulate an operation that fails and is retried
    wish::core::OperationRecord attempt1;
    attempt1.operation_id = "op-backend-connect";
    attempt1.operation_name = "backend.connect";
    attempt1.outcome = wish::core::OperationOutcome::Failed;
    attempt1.retry_count = 0;
    attempt1.max_retries = 3;
    attempt1.error_code = 5001; // kBackendUnavailable
    attempt1.diagnostic_message = "Connection refused";
    history.record_operation(attempt1);

    // Should allow retry (retry_count < max_retries)
    EXPECT_TRUE(history.should_retry("op-backend-connect"));

    // Simulate retry attempt 2 (also fails)
    wish::core::OperationRecord attempt2;
    attempt2.operation_id = "op-backend-connect";
    attempt2.operation_name = "backend.connect";
    attempt2.outcome = wish::core::OperationOutcome::Retrying;
    attempt2.retry_count = 1;
    attempt2.max_retries = 3;
    attempt2.error_code = 5001;
    attempt2.diagnostic_message = "Retry 1: still unavailable";
    history.record_operation(attempt2);

    // Should still allow retry
    EXPECT_TRUE(history.should_retry("op-backend-connect"));

    // Simulate retry attempt 3 (succeeds)
    wish::core::OperationRecord attempt3;
    attempt3.operation_id = "op-backend-connect";
    attempt3.operation_name = "backend.connect";
    attempt3.outcome = wish::core::OperationOutcome::Success;
    attempt3.retry_count = 2;
    attempt3.max_retries = 3;
    history.record_operation(attempt3);

    // Should NOT allow further retry (already succeeded)
    EXPECT_FALSE(history.should_retry("op-backend-connect"));

    std::cout << "  PASS: test_retry_behavior_failure_path\n";
}

void test_retry_exhaustion_failure_path() {
    wish::core::OperationHistory history;

    // Simulate an operation that exhausts all retries
    for (std::uint32_t i = 0; i < 3; ++i) {
        wish::core::OperationRecord attempt;
        attempt.operation_id = "op-exhausted";
        attempt.operation_name = "backend.connect";
        attempt.outcome = (i < 2) ? wish::core::OperationOutcome::Failed
                                  : wish::core::OperationOutcome::Abandoned;
        attempt.retry_count = i;
        attempt.max_retries = 3;
        attempt.error_code = 5001;
        history.record_operation(attempt);
    }

    // Should NOT allow retry after exhaustion
    EXPECT_FALSE(history.should_retry("op-exhausted"));

    // Verify the operation record shows abandonment
    const auto* record = history.find_operation("op-exhausted");
    EXPECT_TRUE(record != nullptr);
    EXPECT_EQ(record->outcome, wish::core::OperationOutcome::Abandoned);

    std::cout << "  PASS: test_retry_exhaustion_failure_path\n";
}

// =========================================================================
// Idempotency — failure-path tests
// =========================================================================

void test_idempotent_operation_detection() {
    wish::core::OperationHistory history;

    // Record a completed operation
    wish::core::OperationRecord original;
    original.operation_id = "op-idempotent-admit";
    original.operation_name = "session.admit";
    original.outcome = wish::core::OperationOutcome::Success;
    original.diagnostic_message = "Player admitted successfully";
    history.record_operation(original);

    // Verify the operation ID is known (idempotency check)
    EXPECT_TRUE(history.has_operation_id("op-idempotent-admit"));

    // Attempt to record the same operation again — should be detected as replay
    EXPECT_TRUE(history.has_operation_id("op-idempotent-admit"));

    // Record the replay with different outcome
    wish::core::OperationRecord replay;
    replay.operation_id = "op-idempotent-admit";
    replay.operation_name = "session.admit";
    replay.outcome = wish::core::OperationOutcome::IdempotentReplay;
    replay.is_idempotent_replay = true;
    replay.diagnostic_message = "Idempotent replay detected: operation already completed";
    history.record_operation(replay);

    // Verify the replay is tracked
    const auto* found = history.find_operation("op-idempotent-admit");
    EXPECT_TRUE(found != nullptr);

    // The most recent record should be the replay
    const auto& ops = history.operations();
    bool found_replay = false;
    for (const auto& op : ops) {
        if (op.is_idempotent_replay) {
            found_replay = true;
            break;
        }
    }
    EXPECT_TRUE(found_replay);

    // Ensure idempotent replays don't create duplicate side effects
    EXPECT_EQ(history.operation_count(), 2); // original + replay

    std::cout << "  PASS: test_idempotent_operation_detection\n";
}

// =========================================================================
// Support bundle — failure-path tests
// =========================================================================

void test_support_bundle_with_failures() {
    wish::core::ServiceIdentity service;
    service.service_name = "wish-engine";
    service.instance_id = "srv-failure-test";
    service.version = "0.1.0";

    // Collect correlated failures
    std::vector<wish::core::CorrelatedFailure> failures;

    // Auth failure
    failures.push_back(wish::core::correlate_failure(
        wish::WishErrorCode::kAuthRejected, 0,
        "wish/session", "Authentication rejected for player-001", false));

    // Backend timeout (recovered)
    failures.push_back(wish::core::correlate_failure(
        wish::WishErrorCode::kBackendTimeout, 1001,
        "wish/integrations", "Nakama backend timeout (AE-NET-0001)", true));

    // Capacity exceeded
    failures.push_back(wish::core::correlate_failure(
        wish::WishErrorCode::kCapacityExceeded, 0,
        "wish/admin", "Server at maximum capacity", false));

    // Build operation history with failure paths
    wish::core::OperationHistory history;
    {
        wish::core::OperationRecord op;
        op.operation_id = "op-fail-001";
        op.operation_name = "auth.validate";
        op.outcome = wish::core::OperationOutcome::Failed;
        op.retry_count = 0;
        op.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
        op.diagnostic_message = "Invalid session token";
        history.record_operation(op);
    }
    {
        wish::core::OperationRecord op;
        op.operation_id = "op-fail-002";
        op.operation_name = "backend.connect";
        op.outcome = wish::core::OperationOutcome::Retrying;
        op.retry_count = 1;
        op.max_retries = 3;
        op.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kBackendUnavailable);
        op.diagnostic_message = "Retry 1: connection refused";
        history.record_operation(op);
    }
    {
        wish::core::OperationRecord op;
        op.operation_id = "op-fail-003";
        op.operation_name = "session.admit";
        op.outcome = wish::core::OperationOutcome::Abandoned;
        op.retry_count = 3;
        op.max_retries = 3;
        op.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded);
        op.diagnostic_message = "Capacity exceeded, abandoning";
        history.record_operation(op);
    }

    // Build the support bundle
    auto bundle = wish::core::build_support_bundle(
        service, "0.1.0", 7200, failures, history,
        "Failure-path diagnostic bundle for Phase 8 testing");

    // Verify bundle contents
    EXPECT_FALSE(bundle.bundle_id.empty());
    EXPECT_EQ(bundle.failures.size(), 3u);
    EXPECT_EQ(bundle.operations.size(), 3u);
    EXPECT_EQ(bundle.service.service_name, "wish-engine");
    EXPECT_EQ(bundle.uptime_seconds, 7200u);

    // Verify specific failure types are present
    bool has_auth = false;
    bool has_backend = false;
    bool has_capacity = false;
    for (const auto& f : bundle.failures) {
        if (f.wish_error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected))
            has_auth = true;
        if (f.wish_error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kBackendTimeout))
            has_backend = true;
        if (f.wish_error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded))
            has_capacity = true;
    }
    EXPECT_TRUE(has_auth);
    EXPECT_TRUE(has_backend);
    EXPECT_TRUE(has_capacity);

    // Verify the formatted bundle is valid
    auto formatted = wish::core::format_support_bundle(bundle);
    EXPECT_FALSE(formatted.empty());
    EXPECT_TRUE(formatted.find(bundle.bundle_id) != std::string::npos);
    EXPECT_TRUE(formatted.find("wish-engine") != std::string::npos);
    EXPECT_TRUE(formatted.find("Failure-path diagnostic bundle") != std::string::npos);

    std::cout << "  PASS: test_support_bundle_with_failures\n";
}

// =========================================================================
// Session lifecycle — failure-path tests
// =========================================================================

void test_flashback_adapter_failure_paths() {
    // Test that the flashback adapter properly handles lifecycle events
    struct TrackingAdapter final : public wish::integrations::flashback::IGameSessionAdapter {
        int admit_events {0};
        int remove_events {0};
        int timeout_events {0};
        int complete_events {0};
        bool can_reconnect_val {true};

        void on_player_admitted(const wish::integrations::flashback::GamePlayerState& /*state*/) override {
            ++admit_events;
        }
        void on_player_removed(std::string_view /*player_id*/) override {
            ++remove_events;
        }
        void on_activity_complete(wish::core::ActivityId /*activity_id*/) override {
            ++complete_events;
        }
        void on_player_timeout(std::string_view /*player_id*/) override {
            ++timeout_events;
        }
        bool can_reconnect(std::string_view /*player_id*/) override {
            return can_reconnect_val;
        }
        std::size_t active_player_count() const override {
            return static_cast<std::size_t>(admit_events - remove_events);
        }
    };

    TrackingAdapter adapter;

    // Admit players
    wish::integrations::flashback::GamePlayerState p1;
    p1.player_id = "p-fail-001";
    p1.session_id = "s-fail-001";
    p1.admitted = true;
    adapter.on_player_admitted(p1);

    wish::integrations::flashback::GamePlayerState p2;
    p2.player_id = "p-fail-002";
    p2.session_id = "s-fail-002";
    p2.admitted = true;
    adapter.on_player_admitted(p2);

    EXPECT_EQ(adapter.admit_events, 2);
    EXPECT_EQ(adapter.active_player_count(), 2u);

    // Remove one player (disconnect)
    adapter.on_player_removed("p-fail-001");
    EXPECT_EQ(adapter.remove_events, 1);
    EXPECT_EQ(adapter.active_player_count(), 1u);

    // Timeout the other player
    adapter.on_player_timeout("p-fail-002");
    EXPECT_EQ(adapter.timeout_events, 1);

    // Activity completes
    adapter.on_activity_complete(1);
    EXPECT_EQ(adapter.complete_events, 1);

    // Test reconnect behavior
    EXPECT_TRUE(adapter.can_reconnect("p-fail-001"));
    adapter.can_reconnect_val = false;
    EXPECT_FALSE(adapter.can_reconnect("p-fail-001"));

    std::cout << "  PASS: test_flashback_adapter_failure_paths\n";
}

// =========================================================================
// Error envelope — failure-path tests
// =========================================================================

void test_error_envelope_retry_fields() {
    // Verify that retry fields are properly serialized for all error types

    struct EnvelopeTestCase {
        wish::WishErrorCode code;
        bool retryable;
        std::uint32_t retry_after;
        const char* message_key;
    };

    const EnvelopeTestCase cases[] = {
        {wish::WishErrorCode::kAuthRejected, true, 0, "errors.auth.rejected"},
        {wish::WishErrorCode::kCapacityExceeded, true, 30, "errors.capacity.exceeded"},
        {wish::WishErrorCode::kBackendUnavailable, true, 60, "errors.backend.unavailable"},
        {wish::WishErrorCode::kInternalError, false, 0, "errors.internal.error"},
        {wish::WishErrorCode::kProtocolVersionMismatch, false, 0, "errors.protocol.version_mismatch"},
    };

    for (const auto& tc : cases) {
        wish::ErrorEnvelope envelope;
        envelope.error_code = static_cast<std::uint32_t>(tc.code);
        envelope.incident_id = "TESTCASE";
        envelope.message_key = tc.message_key;
        envelope.retryable = tc.retryable;
        envelope.retry_after_seconds = tc.retry_after;

        EXPECT_TRUE(envelope.valid());

        auto serialized = wish::serialize_envelope(envelope);
        EXPECT_FALSE(serialized.empty());

        auto deserialized = wish::deserialize_envelope(serialized);
        EXPECT_TRUE(deserialized.has_value());
        EXPECT_EQ(deserialized->retryable, tc.retryable);
        EXPECT_EQ(deserialized->retry_after_seconds, tc.retry_after);
        EXPECT_EQ(deserialized->error_code, static_cast<std::uint32_t>(tc.code));
        EXPECT_EQ(deserialized->message_key, tc.message_key);
    }

    std::cout << "  PASS: test_error_envelope_retry_fields\n";
}

// =========================================================================
// Observability correlation — failure-path tests
// =========================================================================

void test_failure_correlation_with_engine_errors() {
    // Correlate a Wish service error (WS-BAK-5001) with an engine error (AE-NET-0001)
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kBackendUnavailable,
        1001, // AE-NET-0001 mapped as numeric
        "wish/integrations/nakama",
        "Nakama backend unavailable (AE-NET-0001 correlated)",
        false);

    EXPECT_FALSE(failure.correlation_id.empty());
    EXPECT_EQ(failure.wish_error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kBackendUnavailable));
    EXPECT_EQ(failure.engine_error_code, 1001u);
    EXPECT_FALSE(failure.recovered);

    // Verify the formatted output includes both codes
    auto formatted = wish::core::format_correlated_failure(failure);
    EXPECT_TRUE(formatted.find("WS-BAK-5001") != std::string::npos);
    EXPECT_TRUE(formatted.find("AE-???") != std::string::npos);
    EXPECT_TRUE(formatted.find("wish/integrations/nakama") != std::string::npos);

    std::cout << "  PASS: test_failure_correlation_with_engine_errors\n";
}

void test_recovered_failure_tracking() {
    // A failure that was recovered should show [RECOVERED]
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kBackendTimeout,
        0,
        "wish/integrations",
        "Backend timeout - recovered via retry",
        true);

    EXPECT_TRUE(failure.recovered);

    auto formatted = wish::core::format_correlated_failure(failure);
    EXPECT_TRUE(formatted.find("[RECOVERED]") != std::string::npos);

    std::cout << "  PASS: test_recovered_failure_tracking\n";
}

void test_structured_event_with_correlation() {
    wish::core::StructuredLogEvent event;
    event.timestamp = "2026-07-27T12:00:00Z";
    event.level = "ERROR";
    event.component = "wish/session";
    event.correlation_id = "CORR-BACKEND-001";
    event.message = "Backend unavailable after 3 retries";
    event.metadata = {
        {"retry_count", "3"},
        {"max_retries", "3"},
        {"backend", "nakama"},
    };

    auto formatted = wish::core::format_structured_event(event);
    EXPECT_FALSE(formatted.empty());
    EXPECT_TRUE(formatted.find("CORR-BACKEND-001") != std::string::npos);
    EXPECT_TRUE(formatted.find("ERROR") != std::string::npos);
    EXPECT_TRUE(formatted.find("retry_count") != std::string::npos);
    EXPECT_TRUE(formatted.find("3") != std::string::npos);

    std::cout << "  PASS: test_structured_event_with_correlation\n";
}

} // anonymous namespace

int main() {
    std::cout << "=== Failure-Path Tests ===\n\n";

    std::cout << "--- Identity & Authentication failure paths ---\n";
    test_auth_rejection_failure_path();
    test_auth_empty_token_failure_path();

    std::cout << "\n--- Session failure paths ---\n";
    test_session_admission_rejection_failure_path();
    test_capacity_exceeded_failure_path();

    std::cout << "\n--- Protocol failure paths ---\n";
    test_protocol_version_mismatch_failure_path();

    std::cout << "\n--- Backend failure paths ---\n";
    test_backend_unavailable_failure_path();
    test_backend_timeout_failure_path();

    std::cout << "\n--- Retry & Recovery failure paths ---\n";
    test_retry_behavior_failure_path();
    test_retry_exhaustion_failure_path();

    std::cout << "\n--- Idempotency failure paths ---\n";
    test_idempotent_operation_detection();

    std::cout << "\n--- Support Bundle failure paths ---\n";
    test_support_bundle_with_failures();

    std::cout << "\n--- Flashback adapter failure paths ---\n";
    test_flashback_adapter_failure_paths();

    std::cout << "\n--- Error envelope retry fields ---\n";
    test_error_envelope_retry_fields();

    std::cout << "\n--- Observability correlation ---\n";
    test_failure_correlation_with_engine_errors();
    test_recovered_failure_tracking();
    test_structured_event_with_correlation();

    std::cout << "\n";
    if (g_failure_count == 0) {
        std::cout << "All failure-path tests PASSED.\n";
        return 0;
    }

    std::cerr << g_failure_count << " failure-path test(s) FAILED.\n";
    return 1;
}

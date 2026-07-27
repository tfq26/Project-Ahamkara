#include "wish/core/observability.h"
#include "wish/core/error_codes.h"
#include "wish/core/error_envelope.h"

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void test_generate_correlation_id() {
    auto id1 = wish::core::generate_correlation_id("wish/session");
    assert(id1.find("CORR-") == 0);
    assert(id1.size() == 13); // "CORR-" + 8 hex chars

    // Same seed should produce same ID
    auto id2 = wish::core::generate_correlation_id("wish/session");
    assert(id1 == id2);

    // Different seed should produce different ID
    auto id3 = wish::core::generate_correlation_id("ahamkara/game");
    assert(id1 != id3);

    std::cout << "test_generate_correlation_id passed.\n";
}

void test_correlate_failure() {
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kAuthRejected,
        0,
        "wish/session",
        "Authentication token rejected",
        false);

    assert(!failure.correlation_id.empty());
    assert(failure.wish_error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected));
    assert(failure.engine_error_code == 0);
    assert(failure.source_component == "wish/session");
    assert(failure.message == "Authentication token rejected");
    assert(!failure.recovered);

    std::cout << "test_correlate_failure passed.\n";
}

void test_correlate_failure_with_engine_code() {
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kBackendUnavailable,
        1001, // AE-NET-0001
        "wish/integrations",
        "Nakama backend unavailable",
        true);

    assert(failure.engine_error_code == 1001);
    assert(failure.recovered);

    std::cout << "test_correlate_failure_with_engine_code passed.\n";
}

void test_correlate_envelope_failure() {
    wish::ErrorEnvelope envelope;
    envelope.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded);
    envelope.incident_id = "DEADBEEF";
    envelope.message_key = "errors.capacity.exceeded";

    auto failure = wish::core::correlate_envelope_failure(
        envelope, 0, "wish/admin", "Server capacity exceeded");

    assert(failure.correlation_id.find("CORR-DEADBEEF") != std::string::npos);
    assert(failure.wish_error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded));
    assert(failure.message == "Server capacity exceeded");

    std::cout << "test_correlate_envelope_failure passed.\n";
}

void test_format_correlated_failure() {
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kProtocolVersionMismatch,
        0,
        "wish/net",
        "Client protocol version rejected",
        false);

    auto formatted = wish::core::format_correlated_failure(failure);
    assert(formatted.find(failure.correlation_id) != std::string::npos);
    assert(formatted.find("WS-PRO-4002") != std::string::npos);
    assert(formatted.find("wish/net") != std::string::npos);
    assert(formatted.find("[RECOVERED]") == std::string::npos); // not recovered

    std::cout << "test_format_correlated_failure passed.\n";
}

void test_format_correlated_failure_recovered() {
    auto failure = wish::core::correlate_failure(
        wish::WishErrorCode::kBackendTimeout,
        0,
        "wish/integrations",
        "Backend timed out",
        true);

    auto formatted = wish::core::format_correlated_failure(failure);
    assert(formatted.find("[RECOVERED]") != std::string::npos);

    std::cout << "test_format_correlated_failure_recovered passed.\n";
}

void test_structured_log_event() {
    wish::core::StructuredLogEvent event;
    event.timestamp = "2026-07-27T01:00:00Z";
    event.level = "ERROR";
    event.component = "wish/session";
    event.correlation_id = "CORR-ABCD1234";
    event.message = "Session expired";
    event.metadata = {{"session_id", "sess-001"}, {"player_id", "p-001"}};

    auto formatted = wish::core::format_structured_event(event);
    assert(formatted.find("2026-07-27T01:00:00Z") != std::string::npos);
    assert(formatted.find("ERROR") != std::string::npos);
    assert(formatted.find("wish/session") != std::string::npos);
    assert(formatted.find("CORR-ABCD1234") != std::string::npos);
    assert(formatted.find("Session expired") != std::string::npos);
    assert(formatted.find("session_id") != std::string::npos);
    assert(formatted.find("sess-001") != std::string::npos);
    assert(formatted.find("player_id") != std::string::npos);
    assert(formatted.find("p-001") != std::string::npos);

    std::cout << "test_structured_log_event passed.\n";
}

void test_structured_log_event_no_metadata() {
    wish::core::StructuredLogEvent event;
    event.timestamp = "2026-07-27T01:00:00Z";
    event.level = "INFO";
    event.component = "wish/admin";
    event.message = "Server started";

    auto formatted = wish::core::format_structured_event(event);
    assert(formatted.find("INFO") != std::string::npos);
    assert(formatted.find("Server started") != std::string::npos);

    std::cout << "test_structured_log_event_no_metadata passed.\n";
}

} // namespace

int main() {
    test_generate_correlation_id();
    test_correlate_failure();
    test_correlate_failure_with_engine_code();
    test_correlate_envelope_failure();
    test_format_correlated_failure();
    test_format_correlated_failure_recovered();
    test_structured_log_event();
    test_structured_log_event_no_metadata();

    std::cout << "All observability tests passed.\n";
    return 0;
}

#include "wish/core/validation_telemetry.h"
#include "wish/integrations/mock_identity_services.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& msg) {
    std::cerr << "validation_telemetry_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_report_event() {
    wish::integrations::NoopValidationTelemetry telemetry;

    EXPECT(telemetry.total_event_count() == 0, "should start with 0 events");

    wish::core::ValidationEvent event {};
    event.category = wish::core::ValidationCategory::InputValidation;
    event.severity = wish::core::ValidationSeverity::Warning;
    event.player_id = "player-1";
    event.session_id = "session-1";
    event.detail = "Invalid input sequence: 42";
    telemetry.report_event(event);

    EXPECT(telemetry.total_event_count() == 1, "should have 1 event");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Warning) == 1,
           "should have 1 warning");

    std::cout << "test_report_event: ok\n";
    return 0;
}

int test_report_convenience() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(
        wish::core::ValidationCategory::MovementValidation,
        wish::core::ValidationSeverity::Critical,
        "player-2",
        "session-2",
        "Teleport detected: delta 5000 units in 1 tick"
    );

    EXPECT(telemetry.total_event_count() == 1, "should have 1 event");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Critical) == 1,
           "should have 1 critical");

    auto events = telemetry.recent_events(10);
    EXPECT(events.size() == 1, "should have 1 recent event");
    if (!events.empty()) {
        EXPECT(events[0].player_id == "player-2", "player_id should match");
        EXPECT(events[0].category == wish::core::ValidationCategory::MovementValidation,
               "category should match");
    }

    std::cout << "test_report_convenience: ok\n";
    return 0;
}

int test_multiple_severity_counts() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Trace, "p1", "s1", "trace");
    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Warning, "p2", "s2", "warning");
    telemetry.report(wish::core::ValidationCategory::StateValidation,
                     wish::core::ValidationSeverity::Error, "p3", "s3", "error");
    telemetry.report(wish::core::ValidationCategory::ProtocolViolation,
                     wish::core::ValidationSeverity::Critical, "p4", "s4", "critical");

    EXPECT(telemetry.total_event_count() == 4, "should have 4 events");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Trace) == 0,
           "Trace events are not counted");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Warning) == 1,
           "should have 1 warning");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Error) == 1,
           "should have 1 error");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Critical) == 1,
           "should have 1 critical");

    std::cout << "test_multiple_severity_counts: ok\n";
    return 0;
}

int test_snapshot_resets_counters() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Warning, "p1", "s1", "warn1");
    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Error, "p2", "s2", "err1");

    auto snap = telemetry.snapshot();
    EXPECT(snap.total_events == 2, "snapshot should show 2 total events");
    EXPECT(snap.warning_count == 1, "snapshot should show 1 warning");
    EXPECT(snap.error_count == 1, "snapshot should show 1 error");

    // After snapshot, counters should be reset
    EXPECT(telemetry.total_event_count() == 0, "counters should reset after snapshot");
    EXPECT(telemetry.severity_count(wish::core::ValidationSeverity::Warning) == 0,
           "warning count should reset");

    std::cout << "test_snapshot_resets_counters: ok\n";
    return 0;
}

int test_events_by_category() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Warning, "p1", "s1", "input issue");
    telemetry.report(wish::core::ValidationCategory::MovementValidation,
                     wish::core::ValidationSeverity::Critical, "p2", "s2", "speed hack");
    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Error, "p1", "s1", "another input issue");

    auto input_events = telemetry.events_by_category(
        wish::core::ValidationCategory::InputValidation, 10);
    EXPECT(input_events.size() == 2, "should have 2 input validation events");

    auto movement_events = telemetry.events_by_category(
        wish::core::ValidationCategory::MovementValidation, 10);
    EXPECT(movement_events.size() == 1, "should have 1 movement validation event");

    auto auth_events = telemetry.events_by_category(
        wish::core::ValidationCategory::AuthValidation, 10);
    EXPECT(auth_events.empty(), "should have no auth validation events");

    std::cout << "test_events_by_category: ok\n";
    return 0;
}

int test_events_by_player() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Warning, "player-A", "s1", "issue 1");
    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Error, "player-B", "s2", "issue 2");
    telemetry.report(wish::core::ValidationCategory::MovementValidation,
                     wish::core::ValidationSeverity::Critical, "player-A", "s3", "issue 3");

    auto player_a_events = telemetry.events_by_player("player-A", 10);
    EXPECT(player_a_events.size() == 2, "player-A should have 2 events");

    auto player_b_events = telemetry.events_by_player("player-B", 10);
    EXPECT(player_b_events.size() == 1, "player-B should have 1 event");

    auto player_c_events = telemetry.events_by_player("player-C", 10);
    EXPECT(player_c_events.empty(), "player-C should have 0 events");

    std::cout << "test_events_by_player: ok\n";
    return 0;
}

int test_clear_events() {
    wish::integrations::NoopValidationTelemetry telemetry;

    telemetry.report(wish::core::ValidationCategory::InputValidation,
                     wish::core::ValidationSeverity::Warning, "p1", "s1", "test");
    EXPECT(telemetry.total_event_count() == 1, "should have 1 event");

    telemetry.clear();
    EXPECT(telemetry.total_event_count() == 0, "should have 0 events after clear");
    EXPECT(telemetry.recent_events(10).empty(), "recent events should be empty after clear");

    std::cout << "test_clear_events: ok\n";
    return 0;
}

int test_ring_buffer_overflow() {
    wish::integrations::NoopValidationTelemetry telemetry;

    // Report more events than the ring buffer capacity
    constexpr std::size_t kOverflow = wish::integrations::NoopValidationTelemetry::kMaxEvents + 50;
    for (std::size_t i = 0; i < kOverflow; ++i) {
        telemetry.report(wish::core::ValidationCategory::InputValidation,
                         wish::core::ValidationSeverity::Warning,
                         "player-overflow",
                         "session-overflow",
                         "overflow event " + std::to_string(i));
    }

    // Counters should reflect all events
    EXPECT(telemetry.total_event_count() == kOverflow,
           "total count should reflect all events even after overflow");

    // But recent_events should only return up to the buffer capacity
    auto recent = telemetry.recent_events(
        wish::integrations::NoopValidationTelemetry::kMaxEvents + 10);
    EXPECT(recent.size() == wish::integrations::NoopValidationTelemetry::kMaxEvents,
           "should cap at buffer capacity");

    std::cout << "test_ring_buffer_overflow: ok\n";
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_report_event();
    failures += test_report_convenience();
    failures += test_multiple_severity_counts();
    failures += test_snapshot_resets_counters();
    failures += test_events_by_category();
    failures += test_events_by_player();
    failures += test_clear_events();
    failures += test_ring_buffer_overflow();

    if (failures > 0) {
        std::cerr << failures << " validation telemetry test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All validation telemetry tests passed.\n";
    return 0;
}

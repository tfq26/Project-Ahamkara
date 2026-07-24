#pragma once

#include "wish/types.h"

#include <chrono>
#include <string>
#include <vector>

namespace wish::core {

/// Severity level of a validation event.
enum class ValidationSeverity : wish::u8 {
    Trace,     // Routine validation pass
    Warning,   // Suspicious but not definitive
    Error,     // Validation failure
    Critical   // Likely cheating / integrity violation
};

/// Category of validation event.
enum class ValidationCategory : wish::u8 {
    InputValidation,    // Invalid input sequences, out-of-bounds actions
    StateValidation,    // State hash mismatch, desync detection
    RateLimit,          // Excessive requests, spam detection
    ProtocolViolation,  // Unexpected packet structure or sequence
    AuthValidation,     // Token validation, session hijacking attempt
    MovementValidation, // Speed-hack, teleport detection
    IntegrityCheck      // Memory/modification detection
};

/// A single validation event recorded by the system.
struct ValidationEvent {
    ValidationCategory category {ValidationCategory::InputValidation};
    ValidationSeverity severity {ValidationSeverity::Warning};
    std::string player_id;
    std::string session_id;
    std::string detail;                              // Human-readable description
    std::chrono::steady_clock::time_point timestamp {};
    std::uint64_t event_id {0};                      // Monotonically increasing ID
};

/// A snapshot of validation telemetry for periodic reporting.
struct ValidationTelemetrySnapshot {
    std::vector<ValidationEvent> events;
    std::uint64_t total_events {0};
    std::uint64_t critical_count {0};
    std::uint64_t warning_count {0};
    std::uint64_t error_count {0};
    std::chrono::steady_clock::time_point timestamp {};
};

/// Result of a validation check.
struct ValidationResult {
    bool passed {true};
    ValidationSeverity severity {ValidationSeverity::Trace};
    std::string reason;
};

/**
 * @brief Records and reports validation/anti-cheat telemetry events.
 *
 * Provides a service-backed seam for tracking suspicious behavior,
 * input validation failures, and protocol violations. Events are
 * stored in a ring buffer for periodic reporting and diagnostics.
 *
 * This is NOT an anti-cheat enforcement system — it is a passive
 * telemetry collector that feeds into the admin/metrics pipeline.
 */
class ValidationTelemetry {
public:
    virtual ~ValidationTelemetry() = default;

    /// Record a validation event.
    virtual void report_event(const ValidationEvent& event) = 0;

    /// Record a validation event with convenience parameters.
    virtual void report(ValidationCategory category,
                        ValidationSeverity severity,
                        const std::string& player_id,
                        const std::string& session_id,
                        const std::string& detail) = 0;

    /// Get recent events (up to max_count, newest first).
    [[nodiscard]] virtual std::vector<ValidationEvent> recent_events(
        std::size_t max_count = 64) const = 0;

    /// Get events matching a specific category.
    [[nodiscard]] virtual std::vector<ValidationEvent> events_by_category(
        ValidationCategory category,
        std::size_t max_count = 64) const = 0;

    /// Get events for a specific player.
    [[nodiscard]] virtual std::vector<ValidationEvent> events_by_player(
        const std::string& player_id,
        std::size_t max_count = 64) const = 0;

    /// Take a snapshot and reset counters.
    [[nodiscard]] virtual ValidationTelemetrySnapshot snapshot() = 0;

    /// Get total event count since last snapshot.
    [[nodiscard]] virtual std::uint64_t total_event_count() const = 0;

    /// Get count of events at a given severity level.
    [[nodiscard]] virtual std::uint64_t severity_count(ValidationSeverity severity) const = 0;

    /// Clear all recorded events.
    virtual void clear() = 0;
};

} // namespace wish::core

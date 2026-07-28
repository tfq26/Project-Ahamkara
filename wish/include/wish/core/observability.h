#pragma once

#include "wish/core/error_codes.h"
#include "wish/core/error_envelope.h"
#include "wish/types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

/// Maximum length of a correlation ID string.
inline constexpr std::size_t kMaxCorrelationId = 36;

/// Maximum length of a service name string.
inline constexpr std::size_t kMaxServiceName = 32;

/// Stable identifier for a service instance.
/// This is NOT a session or player identifier — it identifies the
/// service/process so log aggregation can group related failures.
struct ServiceIdentity {
    std::string service_name;
    std::string instance_id;
    std::string version;
};

/// A single correlated failure event.
/// Joins an engine-domain error (AE-*) with a service-domain error (WS-*)
/// under a single correlation ID for cross-domain tracing.
struct CorrelatedFailure {
    /// Unique correlation ID for this failure chain (e.g. "CORR-7F4A19C2-A1B2").
    std::string correlation_id;

    /// The WS-* error code from the service domain.
    std::uint32_t wish_error_code {0};

    /// The AE-* error code from the engine domain (0 if not applicable).
    std::uint32_t engine_error_code {0};

    /// Human-readable message for logging.
    std::string message;

    /// Source component (e.g. "wish/session", "ahamkara/game/deathmatch").
    std::string source_component;

    /// Whether this failure was recovered.
    bool recovered {false};
};

/// Structured log event with correlation support.
/// These events are designed to be consumed by structured logging pipelines.
struct StructuredLogEvent {
    /// ISO-8601 timestamp string.
    std::string timestamp;

    /// Log level string ("DEBUG", "INFO", "WARNING", "ERROR").
    std::string level;

    /// Source component.
    std::string component;

    /// Correlation ID linking this event to a failure chain.
    std::string correlation_id;

    /// The log message body.
    std::string message;

    /// Key-value pairs for structured metadata.
    std::vector<std::pair<std::string, std::string>> metadata;
};

/// Generate a correlation ID string.
/// Format: "CORR-XXXXXXXX" where X is a hex digit from the input.
[[nodiscard]] std::string generate_correlation_id(std::string_view seed);

/// Build a CorrelatedFailure from a Wish error code and an optional engine code.
[[nodiscard]] CorrelatedFailure correlate_failure(
    WishErrorCode wish_code,
    std::uint32_t engine_error_code,
    std::string_view source_component,
    std::string_view message,
    bool recovered = false);

/// Build a CorrelatedFailure from an ErrorEnvelope and an optional engine code.
[[nodiscard]] CorrelatedFailure correlate_envelope_failure(
    const ErrorEnvelope& envelope,
    std::uint32_t engine_error_code,
    std::string_view source_component,
    std::string_view message);

/// Format a CorrelatedFailure for structured logging.
[[nodiscard]] std::string format_correlated_failure(const CorrelatedFailure& failure);

/// Format a StructuredLogEvent to a JSON-like string for log output.
[[nodiscard]] std::string format_structured_event(const StructuredLogEvent& event);

} // namespace wish::core

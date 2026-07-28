#pragma once

#include "wish/core/error_codes.h"
#include "wish/core/observability.h"
#include "wish/types.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

/// Maximum entries in an operation history.
inline constexpr std::size_t kMaxOperationHistory = 1024;

/// Maximum length for a bundle ID.
inline constexpr std::size_t kMaxBundleIdLength = 20;

/// The outcome of an operation for retry/idempotency tracking.
enum class OperationOutcome : wish::u8 {
    Success = 0,
    Failed = 1,
    Retrying = 2,
    Abandoned = 3,
    IdempotentReplay = 4,
};

/// A single tracked operation for retry/idempotency support.
struct OperationRecord {
    /// Unique operation ID for idempotency key.
    std::string operation_id;

    /// The operation name (e.g. "auth.validate", "session.admit").
    std::string operation_name;

    /// The outcome of the operation.
    OperationOutcome outcome {OperationOutcome::Success};

    /// Timestamp when the operation was initiated.
    std::chrono::system_clock::time_point timestamp;

    /// Number of retry attempts (0 for first attempt).
    std::uint32_t retry_count {0};

    /// Maximum retries allowed.
    std::uint32_t max_retries {3};

    /// Delay between retries in milliseconds.
    std::uint32_t retry_delay_ms {100};

    /// Error code if the operation failed (0 on success).
    std::uint32_t error_code {0};

    /// Diagnostic message.
    std::string diagnostic_message;

    /// Whether this record represents an idempotent replay.
    bool is_idempotent_replay {false};
};

/// A comprehensive support bundle for troubleshooting.
struct SupportBundle {
    /// Unique bundle identifier.
    std::string bundle_id;

    /// When the bundle was generated.
    std::chrono::system_clock::time_point generated_at;

    /// Server version.
    std::string server_version;

    /// Uptime in seconds.
    std::uint64_t uptime_seconds {0};

    /// Service identity.
    ServiceIdentity service;

    /// List of correlated failures.
    std::vector<CorrelatedFailure> failures;

    /// Operation history for retry/idempotency analysis.
    std::vector<OperationRecord> operations;

    /// Environment variables (names only, values are redacted).
    std::vector<std::string> environment_keys;

    /// List of active sessions at bundle time.
    std::vector<std::string> active_session_ids;

    /// Free-form diagnostic notes.
    std::string notes;
};

/// Operation history tracker with idempotency support.
class OperationHistory {
public:
    /// Record a new operation.
    void record_operation(OperationRecord record);

    /// Check if an operation ID has been seen (for idempotency).
    [[nodiscard]] bool has_operation_id(std::string_view operation_id) const;

    /// Get the result of a previously completed operation by ID.
    [[nodiscard]] const OperationRecord* find_operation(std::string_view operation_id) const;

    /// Check if an operation should be retried (based on retry policy).
    [[nodiscard]] bool should_retry(std::string_view operation_id) const;

    /// Get the number of recorded operations.
    [[nodiscard]] std::size_t operation_count() const;

    /// Get all operations.
    [[nodiscard]] const std::vector<OperationRecord>& operations() const;

    /// Clear all operations.
    void clear();

private:
    std::vector<OperationRecord> operations_;
};

/// Generate a bundle ID string.
[[nodiscard]] std::string generate_bundle_id();

/// Build a support bundle from provided data.
[[nodiscard]] SupportBundle build_support_bundle(
    const ServiceIdentity& service,
    std::string_view server_version,
    std::uint64_t uptime_seconds,
    const std::vector<CorrelatedFailure>& failures,
    const OperationHistory& history,
    std::string_view notes);

/// Format a support bundle to a diagnostic string for logging/export.
[[nodiscard]] std::string format_support_bundle(const SupportBundle& bundle);

/// Format an operation record to a string.
[[nodiscard]] std::string format_operation_record(const OperationRecord& record);

} // namespace wish::core

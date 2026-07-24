#pragma once

/// @file progression_adapter.h
///
/// Flashback progression persistence adapter.
///
/// This is the public boundary between Flashback gameplay code and the Wish
/// persistence service.  It translates Flashback progression data to/from
/// Wish's public MatchResultReporter contract.
///
/// OWNERSHIP: Flashback (game/).  Wish receives only its neutral envelope
/// and opaque/versioned payload.
///
/// The adapter composes:
///   - ProgressionSaveData  — versioned Flashback save schema
///   - RewardManager        — idempotent grant dispatch + offline queue
///   - MatchResultReporter  — Wish persistence interface (injected)

#include "ahamkara/game/adapters/progression_data.h"
#include "ahamkara/game/adapters/reward_manager.h"
#include "wish/core/session_services.h"

#include <memory>
#include <string>
#include <vector>

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// Offline policy summary (see progression_adapter.cpp for full text)
// ---------------------------------------------------------------------------

/// OfflineBehaviour documents what happens when the Wish backend is
/// unreachable during a reward grant attempt.
enum class OfflineBehaviour : std::uint8_t {
    /// Result is queued and retried with exponential backoff.
    /// No progress is silently lost.
    kQueueAndRetry,

    /// Result is rejected immediately; caller may choose to retry later.
    /// Guarantees no in-memory backpressure.
    kReject,

    /// Result is queued until the queue is full, then oldest entries
    /// are evicted.  Backend-unavailable errors are logged but never
    /// cause data loss of the most recent results.
    kQueueWithEviction,
};

/// Returns a human-readable description of the offline behaviour.
[[nodiscard]] const char* offline_behaviour_label(OfflineBehaviour behaviour);

// ---------------------------------------------------------------------------
// ProgressionAdapter
// ---------------------------------------------------------------------------

/// High-level adapter for Flashback progression persistence.
///
/// Usage:
///   1. Create with a shared Wish MatchResultReporter and optional config.
///   2. At match end, call report_result() with the player's outcome.
///   3. On server startup, call restore() with any previously saved queue.
///   4. Periodically call retry_pending() to flush the offline queue.
///   5. On graceful shutdown, call snapshot_pending() to persist the queue.
class ProgressionAdapter {
  public:
    /// Construct the adapter.
    /// @param reporter  Shared Wish MatchResultReporter (must remain valid
    ///                  for the adapter's lifetime).
    /// @param config    Optional reward manager tuning.
    /// @param offline   Desired offline behaviour policy.
    explicit ProgressionAdapter(
        std::shared_ptr<wish::core::MatchResultReporter> reporter,
        RewardManagerConfig config = {},
        OfflineBehaviour offline = OfflineBehaviour::kQueueAndRetry);

    ~ProgressionAdapter() = default;

    // No copying or moving (owns shared state).
    ProgressionAdapter(const ProgressionAdapter&) = delete;
    ProgressionAdapter& operator=(const ProgressionAdapter&) = delete;
    ProgressionAdapter(ProgressionAdapter&&) = delete;
    ProgressionAdapter& operator=(ProgressionAdapter&&) = delete;

    // -- Result reporting ---------------------------------------------------

    /// Report a match/activity result for progression processing.
    ///
    /// This translates Flashback's ProgressionResult into a Wish MatchResult
    /// and submits it through the injected MatchResultReporter.
    ///
    /// Returns a ProgressionErrorCode indicating the outcome.
    /// See GrantOutcome for the fine-grained reward disposition.
    [[nodiscard]] ProgressionErrorCode report_result(
        const ProgressionResult& result);

    /// Translate a ProgressionResult into a Wish MatchResult.
    /// This is the core translation function.
    [[nodiscard]] wish::core::MatchResult to_wish_match_result(
        const ProgressionResult& result) const;

    /// Translate a Wish MatchResult back into a ProgressionResult.
    [[nodiscard]] ProgressionResult from_wish_match_result(
        const wish::core::MatchResult& match) const;

    // -- Offline queue management -------------------------------------------

    /// Retry all pending (queued) results whose retry timer has expired.
    /// Returns the number of results that were retried.
    [[nodiscard]] std::size_t retry_pending();

    /// Return the number of results currently queued for offline retry.
    [[nodiscard]] std::size_t pending_count() const;

    /// Return the number of tracked result IDs (idempotency set size).
    [[nodiscard]] std::size_t tracked_result_count() const;

    // -- Snapshot / restore -------------------------------------------------

    /// Snapshot the pending queue for external persistence.
    [[nodiscard]] std::vector<PendingResult> snapshot_pending() const;

    /// Restore a previously snapshotted pending queue.
    void restore_pending(const std::vector<PendingResult>& pending);

    /// Reset all state (idempotency cache and pending queue).
    void reset();

    /// Override the clock function (for testing).
    void set_clock(RewardManager::ClockFn fn);

    // -- Save-data management -----------------------------------------------

    /// Migrate save data to the latest schema version.
    [[nodiscard]] static ProgressionErrorCode migrate_save_data(
        ProgressionSaveData& data);

    /// Serialise save data for persistent storage.
    [[nodiscard]] static std::vector<std::byte> serialize(
        const ProgressionSaveData& data);

    /// Deserialise save data, running migrations if needed.
    [[nodiscard]] static ProgressionDataResult deserialize(
        const std::vector<std::byte>& bytes);

    /// Update player progression after a completed match.
    /// This is a convenience that computes XP gains, level-ups, etc.
    [[nodiscard]] static ProgressionErrorCode apply_match_outcome(
        const ProgressionResult& result,
        ProgressionSaveData& data);

  private:
    /// Internal grant callback that wraps the Wish MatchResultReporter.
    bool dispatch_to_wish(const ProgressionResult& result);

    std::shared_ptr<wish::core::MatchResultReporter> reporter_;
    RewardManager reward_manager_;
    OfflineBehaviour offline_behaviour_;
};

// ---------------------------------------------------------------------------
// Offline policy — full documentation
// ---------------------------------------------------------------------------

/*
 * Offline / Unavailable Behaviour Policy
 * =======================================
 *
 * When the Wish backend is unreachable, the ProgressionAdapter behaves
 * according to the configured OfflineBehaviour:
 *
 * 1. kQueueAndRetry (default)
 *    - The result is enqueued in an in-memory pending queue.
 *    - Retries happen with exponential backoff:
 *        delay_ms = retry_base_delay_ms * 2^retry_count
 *      with jitter of ±25%.
 *    - After kMaxRetryAttempts (5) failures, the result is permanently
 *      marked as kRetryExhausted and removed from the queue.
 *    - The caller is NOT notified synchronously; the result of retry_pending()
 *      indicates how many entries were retried.
 *    - When the queue reaches kMaxOfflineQueueSize (256), new entries
 *      are rejected with kOfflineQueueFull.
 *
 * 2. kReject
 *    - No queuing.  The grant_reward call fails immediately with
 *      kBackendUnavailable.
 *    - The caller can schedule its own retry strategy.
 *    - This mode is appropriate when results are ephemeral or when
 *      the caller manages its own persistence.
 *
 * 3. kQueueWithEviction
 *    - Like kQueueAndRetry, but when the queue is full the oldest
 *      pending entry is evicted to make room for the new one.
 *    - Evicted entries are logged and counted but their rewards
 *      are silently skipped.
 *    - USE WITH CAUTION: this CAN lose progress.
 *
 * All modes share these invariants:
 *   - A result_id that has already been granted (found in the idempotency
 *     set) will NEVER produce a duplicate grant regardless of mode.
 *   - Corrupt or unparseable backend responses produce an error but do
 *     NOT remove the result from the retry queue (unless max retries
 *     are exceeded).
 *   - Schema version mismatches in save data are caught at deserialisation
 *     time and reported via ProgressionErrorCode BEFORE any grant attempt.
 *   - The adapter logs all state transitions for diagnostics.
 */

}  // namespace ahamkara::game::adapters

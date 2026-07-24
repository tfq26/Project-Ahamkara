#pragma once

/// @file reward_manager.h
///
/// Idempotent reward grant manager for Flashback progression.
///
/// The RewardManager ensures:
///   1. No reward is granted more than once per result_id (idempotency).
///   2. Results are queued locally when the Wish backend is unavailable.
///   3. Queued results are retried with exponential backoff.
///   4. Retry exhaustion is explicitly reported, never silently dropped.
///
/// The manager is purely in-memory.  Callers may snapshot the pending queue
/// for external persistence (e.g. to disk on graceful shutdown) and restore
/// it on startup.
///
/// Thread-safety: NOT thread-safe.  Call from a single logical thread
/// (e.g. the server tick loop or a dedicated persistence worker).

#include "ahamkara/game/adapters/progression_data.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Tunable parameters for the RewardManager.
struct RewardManagerConfig {
    /// Maximum number of result IDs remembered for idempotency checks.
    std::size_t max_recent_ids {kMaxRecentResultIds};

    /// Maximum number of results queued for offline retry.
    std::size_t max_offline_queue {kMaxOfflineQueueSize};

    /// Maximum retry attempts per result before giving up.
    int max_retry_attempts {kMaxRetryAttempts};

    /// Base delay in milliseconds (first retry wait).
    std::int64_t retry_base_delay_ms {kRetryBaseDelayMs};
};

// ---------------------------------------------------------------------------
// Reward grant callback
// ---------------------------------------------------------------------------

/// Signature for the function that actually delivers a reward to the Wish
/// backend (or equivalent persistence store) via the MatchResultReporter.
///
/// The callback MUST be idempotent: calling it twice with the same result_id
/// MUST NOT grant the reward twice.
///
/// @param result   The progression result to persist.
/// @return         true if the backend accepted the result,
///                 false if it should be retried later.
using RewardGrantCallback = std::function<bool(const ProgressionResult& result)>;

// ---------------------------------------------------------------------------
// Pending result (for offline queue)
// ---------------------------------------------------------------------------

struct PendingResult {
    ProgressionResult result {};
    int retry_count {0};
    std::int64_t next_retry_time_ms {0};  ///< Steady clock epoch ms
};

// ---------------------------------------------------------------------------
// RewardManager
// ---------------------------------------------------------------------------

class RewardManager {
  public:
    explicit RewardManager(RewardManagerConfig config = {});

    /// Attempt to grant a reward for the given result.
    ///
    /// Returns:
    ///   kAlreadyGranted - reward was already granted for this result_id
    ///   kGranted        - reward granted via the callback
    ///   kPending        - backend unavailable; result queued for retry
    ///   kRetryExhausted - max retries reached; reward not granted
    ///   kFailed         - unrecoverable failure
    ///
    /// The callback is invoked inline when the backend is reachable.
    [[nodiscard]] GrantOutcome grant_reward(
        const ProgressionResult& result,
        const RewardGrantCallback& grant_callback);

    /// Retry all pending results whose next_retry_time has elapsed.
    /// Returns the number of results that were retried.
    [[nodiscard]] std::size_t retry_pending(
        const RewardGrantCallback& grant_callback);

    /// Check whether a given result_id has already been granted.
    [[nodiscard]] bool is_already_granted(const std::string& result_id) const;

    /// Return the number of results currently queued for retry.
    [[nodiscard]] std::size_t pending_count() const;

    /// Return the number of distinct result IDs tracked for idempotency.
    [[nodiscard]] std::size_t tracked_count() const;

    // -- Snapshot / restore for durable offline queue -----------------------

    /// Snapshot the pending queue for external persistence.
    [[nodiscard]] std::vector<PendingResult> snapshot_pending() const;

    /// Restore a previously snapshotted pending queue.
    void restore_pending(const std::vector<PendingResult>& pending);

    /// Reset all state (clear idempotency set and pending queue).
    void reset();

    /// Return the current clock epoch milliseconds (overridable in tests).
    using ClockFn = std::function<std::int64_t()>;
    void set_clock(ClockFn fn);

  private:
    bool insert_recent_id(const std::string& result_id);

    RewardManagerConfig config_;
    std::vector<std::string> recent_result_ids_;  // bounded LRU-ish set
    std::vector<PendingResult> pending_queue_;
    ClockFn clock_fn_;
};

}  // namespace ahamkara::game::adapters

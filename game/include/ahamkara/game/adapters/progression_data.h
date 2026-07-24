#pragma once

/// @file progression_data.h
///
/// Versioned Flashback progression save data format.
///
/// Owned by Flashback.  Wish receives only an opaque/versioned payload
/// through its public MatchResultReporter interface.  The version field
/// enables forward/backward migration without shared schema definitions.
///
/// Policy:
///   - Every schema change bumps kCurrentVersion and MUST add a migration
///     function before the old version is removed.
///   - The migration function signature is:
///       bool migrate_vN_to_vNp1(ProgressionSaveData&);
///   - Migration functions are tested in progression_adapter_tests.cpp.

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// Error codes for progression operations
// ---------------------------------------------------------------------------

/// Flashback progression error domain string (FB-PRO).
inline constexpr const char* kProgressionDomain = "PRO";

/// Stable numeric codes for Flashback progression operations.
/// Range 2001-2099 reserved for progression subsystem.
enum class ProgressionErrorCode : std::uint32_t {
    kSuccess = 0,
    kDataVersionUnknown = 2001,   ///< Save data version is not recognised
    kDataCorrupt = 2002,          ///< Save data failed integrity check
    kMigrationFailed = 2003,      ///< Schema migration could not complete
    kResultIdConflict = 2004,     ///< Duplicate result_id detected
    kBackendUnavailable = 2005,   ///< Wish persistence backend unreachable
    kRetryExhausted = 2006,       ///< Max retry attempts consumed
    kOfflineQueueFull = 2007,     ///< Offline result queue capacity exceeded
    kInvalidArgument = 2008,      ///< Bad parameters supplied
    kInternalError = 2099,        ///< Internal/system failure
};

/// Returns a human-readable label for a ProgressionErrorCode.
inline constexpr const char* progression_error_label(ProgressionErrorCode code) {
    switch (code) {
    case ProgressionErrorCode::kSuccess:           return "success";
    case ProgressionErrorCode::kDataVersionUnknown: return "data_version_unknown";
    case ProgressionErrorCode::kDataCorrupt:       return "data_corrupt";
    case ProgressionErrorCode::kMigrationFailed:   return "migration_failed";
    case ProgressionErrorCode::kResultIdConflict:  return "result_id_conflict";
    case ProgressionErrorCode::kBackendUnavailable: return "backend_unavailable";
    case ProgressionErrorCode::kRetryExhausted:    return "retry_exhausted";
    case ProgressionErrorCode::kOfflineQueueFull:  return "offline_queue_full";
    case ProgressionErrorCode::kInvalidArgument:   return "invalid_argument";
    case ProgressionErrorCode::kInternalError:     return "internal_error";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Flashback progression result — the per-match/per-activity outcome that
// Wish persists on behalf of Flashback.
// ---------------------------------------------------------------------------

/// A single match or activity result to be reported to the Wish backend.
/// The result_id provides idempotency: Wish (or the adapter) MUST NOT grant
/// rewards for the same result_id more than once.
struct ProgressionResult {
    /// Globally unique result identifier (UUID string or server-assigned token).
    /// Used for idempotent reward grant deduplication.
    std::string result_id {};

    /// Activity/mode identifier (e.g. "deathmatch", "horde", "social_hub").
    std::string activity_id {};

    /// Player identifier that earned this result.
    std::string player_id {};

    /// The player's final score in this activity.
    std::int64_t score {0};

    /// Whether the player completed the activity (as opposed to disconnecting).
    bool completed {false};

    /// Duration of the activity in seconds.
    float duration_seconds {0.0F};

    /// Custom key/value metadata (game-specific, opaque to Wish).
    /// Examples: "wave_reached:15", "headshots:23", "accuracy:0.67".
    std::vector<std::pair<std::string, std::string>> metadata {};

    /// Server timestamp (steady clock epoch milliseconds) when the result
    /// was generated.  Used for ordering and staleness checks.
    std::int64_t timestamp_ms {0};
};

// ---------------------------------------------------------------------------
// Reward grant record — tracks whether a reward has been granted for a result.
// ---------------------------------------------------------------------------

/// Outcome of a reward grant attempt.
enum class GrantOutcome : std::uint8_t {
    kGranted,          ///< Reward was successfully granted.
    kAlreadyGranted,   ///< Reward was already granted (idempotent dedup).
    kPending,          ///< Queued for later retry (offline / unavailable).
    kFailed,           ///< Grant failed and will not be retried.
    kRetryExhausted,   ///< All retry attempts consumed.
};

inline constexpr const char* grant_outcome_label(GrantOutcome outcome) {
    switch (outcome) {
    case GrantOutcome::kGranted:        return "granted";
    case GrantOutcome::kAlreadyGranted: return "already_granted";
    case GrantOutcome::kPending:        return "pending";
    case GrantOutcome::kFailed:         return "failed";
    case GrantOutcome::kRetryExhausted: return "retry_exhausted";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Versioned progression save data
// ---------------------------------------------------------------------------

/// Maximum number of recent result IDs retained for idempotency checks.
inline constexpr std::size_t kMaxRecentResultIds = 1024;

/// Maximum number of results queued for offline retry.
inline constexpr std::size_t kMaxOfflineQueueSize = 256;

/// Maximum retry attempts per result before giving up.
inline constexpr int kMaxRetryAttempts = 5;

/// Base retry delay in milliseconds (exponential backoff).
inline constexpr int kRetryBaseDelayMs = 1000;

/// Player progression snapshot — reflects the player's state after a match.
struct PlayerProgression {
    std::string player_id {};
    std::int64_t xp {0};
    std::int64_t level {1};
    std::int64_t currency_hard {0};   // premium currency
    std::int64_t currency_soft {0};   // earnable currency
    std::int64_t total_matches_played {0};
    std::int64_t total_wins {0};

    bool operator==(const PlayerProgression& other) const {
        return player_id == other.player_id &&
               xp == other.xp &&
               level == other.level &&
               currency_hard == other.currency_hard &&
               currency_soft == other.currency_soft &&
               total_matches_played == other.total_matches_played &&
               total_wins == other.total_wins;
    }

    bool operator!=(const PlayerProgression& other) const {
        return !(*this == other);
    }
};

/// An unlocked item or entitlement.
struct UnlockedItem {
    std::string item_id {};
    std::string item_type {};     // "weapon", "skin", "emote", "charm", etc.
    std::int64_t unlock_time_ms {0};  // when it was unlocked
};

/// Versioned save data envelope.
/// kCurrentVersion is bumped on each breaking schema change.
struct ProgressionSaveData {
    /// Schema version.  kCurrentVersion is the latest; older versions are
    /// migrated forward on load.
    static constexpr std::uint32_t kCurrentVersion = 1;

    std::uint32_t version {kCurrentVersion};
    PlayerProgression player {};
    std::vector<UnlockedItem> unlocked_items {};
    std::vector<std::string> recent_result_ids {};  // for idempotency
    std::int64_t last_saved_ms {0};

    /// Integrity checksum (simple additive checksum of version + player data).
    /// 0 = not computed.
    std::uint64_t checksum {0};
};

// ---------------------------------------------------------------------------
// Serialisation helpers (binary format, not JSON — no external dep.)
// ---------------------------------------------------------------------------

/// Compute a simple checksum over the progression save data.
/// Not cryptographically secure — sufficient for integrity detection.
[[nodiscard]] std::uint64_t compute_progression_checksum(
    const ProgressionSaveData& data);

/// Check whether the stored checksum matches a recomputation.
/// Returns true if checksum is valid (or was never set).
[[nodiscard]] bool verify_progression_checksum(
    const ProgressionSaveData& data);

/// Serialise ProgressionSaveData to a byte vector (binary format).
[[nodiscard]] std::vector<std::byte> serialize_progression_data(
    const ProgressionSaveData& data);

/// Deserialise ProgressionSaveData from a byte vector.
/// The result contains the deserialised data on success, or an error code
/// on failure.
struct ProgressionDataResult {
    ProgressionSaveData data {};
    ProgressionErrorCode error {ProgressionErrorCode::kSuccess};
    bool ok {false};
};

[[nodiscard]] ProgressionDataResult deserialize_progression_data(
    const std::vector<std::byte>& bytes);

// ---------------------------------------------------------------------------
// Migration helpers
// ---------------------------------------------------------------------------

/// Migrate save data to the latest version.
/// Returns kSuccess if data is already current or migration succeeded.
/// Returns kDataVersionUnknown if the stored version is unrecognised.
/// Returns kMigrationFailed if an intermediate migration step failed.
[[nodiscard]] ProgressionErrorCode migrate_to_latest(
    ProgressionSaveData& data);

}  // namespace ahamkara::game::adapters

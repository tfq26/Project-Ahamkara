#include "ahamkara/game/adapters/progression_adapter.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace ahamkara::game::adapters {

// ===========================================================================
// OfflineBehaviour helpers
// ===========================================================================

const char* offline_behaviour_label(OfflineBehaviour behaviour) {
    switch (behaviour) {
    case OfflineBehaviour::kQueueAndRetry:     return "queue_and_retry";
    case OfflineBehaviour::kReject:            return "reject";
    case OfflineBehaviour::kQueueWithEviction: return "queue_with_eviction";
    }
    return "unknown";
}

// ===========================================================================
// Checksum helpers
// ===========================================================================

// Simple additive checksum: XOR-rotate of integer representations of fields.
// This is NOT cryptographically secure; it detects accidental corruption.
static std::uint64_t compute_checksum_impl(const ProgressionSaveData& data) {
    std::uint64_t cs = 0;

    // Helper: mix in a 64-bit value.
    auto mix = [&cs](std::uint64_t v) {
        cs ^= v;
        cs = (cs << 7) | (cs >> 57);  // rotate
    };

    mix(static_cast<std::uint64_t>(data.version));
    mix(static_cast<std::uint64_t>(data.player.xp));
    mix(static_cast<std::uint64_t>(data.player.level));
    mix(static_cast<std::uint64_t>(data.player.currency_hard));
    mix(static_cast<std::uint64_t>(data.player.currency_soft));
    mix(static_cast<std::uint64_t>(data.player.total_matches_played));
    mix(static_cast<std::uint64_t>(data.player.total_wins));
    mix(static_cast<std::uint64_t>(data.unlocked_items.size()));
    for (const auto& item : data.unlocked_items) {
        // Hash the item_id string into the checksum
        std::uint64_t h = 0;
        for (char c : item.item_id) {
            h = h * 31 + static_cast<std::uint8_t>(c);
        }
        mix(h);
    }
    mix(static_cast<std::uint64_t>(data.recent_result_ids.size()));
    mix(static_cast<std::uint64_t>(data.last_saved_ms));

    return cs;
}

std::uint64_t compute_progression_checksum(const ProgressionSaveData& data) {
    return compute_checksum_impl(data);
}

bool verify_progression_checksum(const ProgressionSaveData& data) {
    if (data.checksum == 0) return true;  // not set — skip
    return data.checksum == compute_checksum_impl(data);
}

// ===========================================================================
// Binary serialisation (simple TLV-like format)
// ===========================================================================
//
// Format (all values are little-endian):
//   - Magic:     "FBPR" (4 bytes)
//   - Version:   uint32
//   - Body:      version-defined byte sequence
//   - Checksum:  uint64
//
// Version 1 body layout:
//   - player_id length  : uint32
//   - player_id         : UTF-8 bytes
//   - xp                : int64
//   - level             : int64
//   - currency_hard     : int64
//   - currency_soft     : int64
//   - total_matches     : int64
//   - total_wins        : int64
//   - item_count        : uint32
//   - per item:
//       - item_id length  : uint32
//       - item_id         : bytes
//       - item_type length: uint32
//       - item_type       : bytes
//       - unlock_time     : int64
//   - recent_ids_count  : uint32
//   - per id:
//       - id length       : uint32
//       - id              : bytes
//   - last_saved_ms     : int64

static constexpr std::uint32_t kSerializationMagic = 0x52504246;  // "FBPR"

template <typename T>
static void write_le(std::vector<std::byte>& buf, T value) {
    auto* p = reinterpret_cast<const std::byte*>(&value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        buf.push_back(p[i]);
    }
}

template <typename T>
static T read_le(const std::byte*& ptr, const std::byte* end) {
    T value = 0;
    if (ptr + sizeof(T) > end) return value;
    auto* p = reinterpret_cast<std::byte*>(&value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        p[i] = *ptr++;
    }
    return value;
}

static void write_string(std::vector<std::byte>& buf, const std::string& s) {
    write_le<std::uint32_t>(buf, static_cast<std::uint32_t>(s.size()));
    for (char c : s) {
        buf.push_back(static_cast<std::byte>(c));
    }
}

static std::string read_string(const std::byte*& ptr, const std::byte* end) {
    auto len = read_le<std::uint32_t>(ptr, end);
    if (ptr + len > end) {
        ptr = end;
        return {};
    }
    std::string s;
    s.reserve(len);
    for (std::uint32_t i = 0; i < len; ++i) {
        s.push_back(static_cast<char>(*ptr++));
    }
    return s;
}

std::vector<std::byte> serialize_progression_data(const ProgressionSaveData& data) {
    std::vector<std::byte> buf;
    buf.reserve(256);

    // Magic
    write_le<std::uint32_t>(buf, kSerializationMagic);
    // Version
    write_le<std::uint32_t>(buf, data.version);
    // Body (version 1)
    write_string(buf, data.player.player_id);
    write_le<std::int64_t>(buf, data.player.xp);
    write_le<std::int64_t>(buf, data.player.level);
    write_le<std::int64_t>(buf, data.player.currency_hard);
    write_le<std::int64_t>(buf, data.player.currency_soft);
    write_le<std::int64_t>(buf, data.player.total_matches_played);
    write_le<std::int64_t>(buf, data.player.total_wins);

    write_le<std::uint32_t>(buf, static_cast<std::uint32_t>(data.unlocked_items.size()));
    for (const auto& item : data.unlocked_items) {
        write_string(buf, item.item_id);
        write_string(buf, item.item_type);
        write_le<std::int64_t>(buf, item.unlock_time_ms);
    }

    write_le<std::uint32_t>(buf, static_cast<std::uint32_t>(data.recent_result_ids.size()));
    for (const auto& id : data.recent_result_ids) {
        write_string(buf, id);
    }

    write_le<std::int64_t>(buf, data.last_saved_ms);

    // Checksum
    std::uint64_t cs = compute_checksum_impl(data);
    write_le<std::uint64_t>(buf, cs);

    return buf;
}

ProgressionDataResult deserialize_progression_data(const std::vector<std::byte>& bytes) {
    ProgressionDataResult result{};

    if (bytes.size() < 8) {
        result.error = ProgressionErrorCode::kDataCorrupt;
        return result;
    }

    const std::byte* ptr = bytes.data();
    const std::byte* end = ptr + bytes.size();

    // Magic
    auto magic = read_le<std::uint32_t>(ptr, end);
    if (magic != kSerializationMagic) {
        result.error = ProgressionErrorCode::kDataCorrupt;
        return result;
    }

    // Version
    auto version = read_le<std::uint32_t>(ptr, end);
    result.data.version = version;

    if (version > ProgressionSaveData::kCurrentVersion) {
        result.error = ProgressionErrorCode::kDataVersionUnknown;
        return result;
    }

    // Body parsing depends on version
    if (version == 1) {
        result.data.player.player_id = read_string(ptr, end);
        result.data.player.xp = read_le<std::int64_t>(ptr, end);
        result.data.player.level = read_le<std::int64_t>(ptr, end);
        result.data.player.currency_hard = read_le<std::int64_t>(ptr, end);
        result.data.player.currency_soft = read_le<std::int64_t>(ptr, end);
        result.data.player.total_matches_played = read_le<std::int64_t>(ptr, end);
        result.data.player.total_wins = read_le<std::int64_t>(ptr, end);

        auto item_count = read_le<std::uint32_t>(ptr, end);
        for (std::uint32_t i = 0; i < item_count; ++i) {
            UnlockedItem item;
            item.item_id = read_string(ptr, end);
            item.item_type = read_string(ptr, end);
            item.unlock_time_ms = read_le<std::int64_t>(ptr, end);
            result.data.unlocked_items.push_back(std::move(item));
        }

        auto id_count = read_le<std::uint32_t>(ptr, end);
        for (std::uint32_t i = 0; i < id_count; ++i) {
            result.data.recent_result_ids.push_back(read_string(ptr, end));
        }

        result.data.last_saved_ms = read_le<std::int64_t>(ptr, end);
    }

    // Checksum (trailer)
    if (ptr + sizeof(std::uint64_t) <= end) {
        result.data.checksum = read_le<std::uint64_t>(ptr, end);
    }

    // Verify integrity
    if (!verify_progression_checksum(result.data)) {
        result.error = ProgressionErrorCode::kDataCorrupt;
        return result;
    }

    // Migrate if needed
    auto migrate_err = migrate_to_latest(result.data);
    if (migrate_err != ProgressionErrorCode::kSuccess) {
        result.error = migrate_err;
        return result;
    }

    result.ok = true;
    result.error = ProgressionErrorCode::kSuccess;
    return result;
}

// ===========================================================================
// Migration
// ===========================================================================

ProgressionErrorCode migrate_to_latest(ProgressionSaveData& data) {
    // Version 1 is the current version — no migration needed.
    if (data.version == ProgressionSaveData::kCurrentVersion) {
        return ProgressionErrorCode::kSuccess;
    }

    // Unknown version (future data)
    if (data.version > ProgressionSaveData::kCurrentVersion) {
        return ProgressionErrorCode::kDataVersionUnknown;
    }

    // If version is 0 (uninitialised), set to current.
    if (data.version == 0) {
        data.version = ProgressionSaveData::kCurrentVersion;
        return ProgressionErrorCode::kSuccess;
    }

    // Version < 1 is not in the supported range (version 1 is the first).
    return ProgressionErrorCode::kDataVersionUnknown;
}

// ===========================================================================
// ProgressionAdapter implementation
// ===========================================================================

ProgressionAdapter::ProgressionAdapter(
    std::shared_ptr<wish::core::MatchResultReporter> reporter,
    RewardManagerConfig config,
    OfflineBehaviour offline)
    : reporter_(std::move(reporter))
    , reward_manager_(config)
    , offline_behaviour_(offline) {
}

ProgressionErrorCode ProgressionAdapter::report_result(
    const ProgressionResult& result) {
    // Validate input
    if (result.result_id.empty() || result.player_id.empty()) {
        return ProgressionErrorCode::kInvalidArgument;
    }

    // Create the grant callback that wraps Wish MatchResultReporter
    RewardGrantCallback callback = [this](const ProgressionResult& r) -> bool {
        return dispatch_to_wish(r);
    };

    auto outcome = reward_manager_.grant_reward(result, callback);

    switch (outcome) {
    case GrantOutcome::kGranted:
    case GrantOutcome::kAlreadyGranted:
        return ProgressionErrorCode::kSuccess;
    case GrantOutcome::kPending:
        return ProgressionErrorCode::kBackendUnavailable;
    case GrantOutcome::kRetryExhausted:
        return ProgressionErrorCode::kRetryExhausted;
    case GrantOutcome::kFailed:
        return ProgressionErrorCode::kInternalError;
    }
    return ProgressionErrorCode::kInternalError;
}

bool ProgressionAdapter::dispatch_to_wish(const ProgressionResult& result) {
    if (!reporter_) return false;

    auto match_result = to_wish_match_result(result);
    try {
        reporter_->report_match_result(match_result);
        return true;
    } catch (...) {
        // Backend unavailable or reporter threw
        return false;
    }
}

wish::core::MatchResult ProgressionAdapter::to_wish_match_result(
    const ProgressionResult& result) const {
    wish::core::MatchResult match{};
    match.match_id = result.result_id;  // use the idempotent ID
    match.player_id = result.player_id;
    match.completed = result.completed;
    match.summary = result.activity_id + ":" + std::to_string(result.score);
    return match;
}

ProgressionResult ProgressionAdapter::from_wish_match_result(
    const wish::core::MatchResult& match) const {
    ProgressionResult result{};
    result.result_id = match.match_id;
    result.player_id = match.player_id;
    result.completed = match.completed;

    // Parse summary back into activity_id and score
    auto colon_pos = match.summary.find(':');
    if (colon_pos != std::string::npos) {
        result.activity_id = match.summary.substr(0, colon_pos);
        try {
            result.score = static_cast<std::int64_t>(
                std::stoll(match.summary.substr(colon_pos + 1)));
        } catch (...) {
            result.score = 0;
        }
    } else {
        result.activity_id = match.summary;
    }

    return result;
}

std::size_t ProgressionAdapter::retry_pending() {
    RewardGrantCallback callback = [this](const ProgressionResult& r) -> bool {
        return dispatch_to_wish(r);
    };
    return reward_manager_.retry_pending(callback);
}

std::size_t ProgressionAdapter::pending_count() const {
    return reward_manager_.pending_count();
}

std::size_t ProgressionAdapter::tracked_result_count() const {
    return reward_manager_.tracked_count();
}

std::vector<PendingResult> ProgressionAdapter::snapshot_pending() const {
    return reward_manager_.snapshot_pending();
}

void ProgressionAdapter::restore_pending(const std::vector<PendingResult>& pending) {
    reward_manager_.restore_pending(pending);
}

void ProgressionAdapter::reset() {
    reward_manager_.reset();
}

void ProgressionAdapter::set_clock(RewardManager::ClockFn fn) {
    reward_manager_.set_clock(std::move(fn));
}

ProgressionErrorCode ProgressionAdapter::migrate_save_data(
    ProgressionSaveData& data) {
    return migrate_to_latest(data);
}

std::vector<std::byte> ProgressionAdapter::serialize(
    const ProgressionSaveData& data) {
    return serialize_progression_data(data);
}

ProgressionDataResult ProgressionAdapter::deserialize(
    const std::vector<std::byte>& bytes) {
    return deserialize_progression_data(bytes);
}

ProgressionErrorCode ProgressionAdapter::apply_match_outcome(
    const ProgressionResult& result,
    ProgressionSaveData& data) {
    if (!result.completed) return ProgressionErrorCode::kSuccess;

    // Base XP: score * 10
    std::int64_t xp_gain = result.score * 10;
    if (xp_gain < 0) xp_gain = 0;

    data.player.xp += xp_gain;
    data.player.total_matches_played++;

    // Simple level-up: every 1000 XP
    data.player.level = 1 + (data.player.xp / 1000);

    // Soft currency: 1 per 10 score
    data.player.currency_soft += result.score / 10;

    return ProgressionErrorCode::kSuccess;
}

}  // namespace ahamkara::game::adapters

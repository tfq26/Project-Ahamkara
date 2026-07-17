#pragma once

#include "ae/core/types.h"
#include "ae/network/server_history.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <cstdint>
#include <limits>
#include <unordered_set>

namespace ahamkara::game {

/**
 * @brief Why a rewind / hit-validation request was rejected.
 *
 * Every rejected path is distinct so server operators can distinguish
 * legitimate latency from abuse or a misconfigured client.
 */
enum class HitReject : ae::u8 {
    None,        ///< Not rejected — clean hit.
    NoHistory,   ///< History buffer is empty.
    FutureTick,  ///< Client tick is ahead of the server (impossible).
    Expired,     ///< Client tick is older than the retention window.
    OutOfWindow, ///< Rewind delta exceeds the maximum allowed window.
    Duplicate,   ///< This hit was already validated.
};

/**
 * @brief Result of a single rewind-and-validate operation.
 *
 * The result is purely a query: it does NOT mutate the history buffer
 * or the live simulation world.
 */
struct HitResult {
    bool hit {false};
    HitReject reject_reason {HitReject::None};
    int hit_dummy_idx {-1};
    Vec3 hit_position {};
    float damage {0.0F};
    bool is_headshot {false};
};

/**
 * @brief Per-client clock tracker that converts client-reported ticks
 *        into server-authoritative ticks with bounded rewind.
 *
 * The server maintains one ServerClockTracker per connected client.
 * It tracks the most recent server tick the client has acknowledged
 * and uses that to clamp rewinds within a fixed maximum window.
 */
class ServerClockTracker {
  public:
    static constexpr float kDefaultMaxRewindMs = 200.0F; // 200 ms maximum rewind.
    static constexpr float kDefaultTickRateHz = 60.0F;
    static constexpr ae::u32 kMaxRewindTicks =
        static_cast<ae::u32>(kDefaultMaxRewindMs * kDefaultTickRateHz / 1000.0F); // 12 ticks

    /**
     * @brief Record a new server tick for this client.
     *
     * Called each server tick to track the highest server tick the
     * client could have plausibly observed.  This is the ceiling for
     * rewind.
     */
    void record_server_tick(ae::u32 server_tick) {
        latest_server_tick_ = server_tick;
    }

    /**
     * @brief Convert a client-reported tick into a server tick for rewind.
     *
     * @param client_tick   The tick the client says they fired on.
     * @param client_rtt    The client's estimated round-trip time (seconds).
     * @return The clamped server tick to query, or UINT32_MAX if the
     *         request is out of bounds.
     */
    [[nodiscard]] ae::u32 convert(ae::u32 client_tick, float client_rtt) const;

    /**
     * @brief Check whether rewind is allowed for a given pair of ticks.
     *
     * @return HitReject::None if the rewind is valid, or the specific
     *         rejection reason.
     */
    [[nodiscard]] HitReject validate_rewind(ae::u32 client_tick, ae::u32 server_tick) const;

    /**
     * @brief Reset tracker state (e.g. on client disconnect/reconnect).
     */
    void reset() {
        latest_server_tick_ = 0;
    }

    [[nodiscard]] ae::u32 latest_server_tick() const {
        return latest_server_tick_;
    }

  private:
    ae::u32 latest_server_tick_ {0};
};

/**
 * @brief The Flashback server-side rewind hit validation engine.
 *
 * Wraps a reference to the server's history buffer and provides safe,
 * query-only hit validation against historical dummy positions.
 *
 * The rewind service does NOT own the history buffer — the caller
 * (e.g. DeathmatchActivity) owns the buffer and passes it by reference.
 * This keeps Flashback code consuming public Ahamkara history contracts
 * without owning them.
 */
class RewindValidation {
  public:
    /**
     * @param history  Reference to the authoritative ServerHistoryBuffer
     *                 maintained by the activity / server tick loop.
     */
    explicit RewindValidation(
        const ae::ServerHistoryBuffer<HistoricalState, 1024>& history);

    /**
     * @brief Validate a hit at the given historical tick.
     *
     * Queries the history buffer, retrieves the historical positions of
     * all dummies, and runs a ray-vs-capsule intersection test against
     * the historical state.  The live world is NOT modified.
     *
     * @param server_tick  The server tick to rewind to (already converted
     *                     and validated by ServerClockTracker).
     * @param origin       Ray origin (camera position at fire time).
     * @param forward      Ray direction (normalised).
     * @param base_damage  Raw weapon damage before multipliers.
     * @param headshot_multiplier  Headshot damage multiplier.
     * @return HitResult describing whether and how the shot landed.
     */
    [[nodiscard]] HitResult validate_hit(
        ae::u32 server_tick,
        const Vec3& origin,
        const Vec3& forward,
        float base_damage,
        float headshot_multiplier) const;

    /**
     * @brief Mark a (client_tick, dummy_idx) pair as processed so the
     *        Duplicate guard can reject it on re-validation.
     */
    void mark_processed(ae::u32 client_tick, int dummy_idx);

    /**
     * @brief Clear all processed-hit tracking (e.g. at match restart).
     */
    void clear_processed();

    /**
     * @brief Max history lookup range (world units).
     */
    static constexpr float kHitscanRange = 1000.0F;

    /**
     * @brief Dummy hitbox constants.
     */
    static constexpr float kDummyRadius = 0.35F;
    static constexpr float kDummyHalfHeight = 1.0F;

  private:
    const ae::ServerHistoryBuffer<HistoricalState, 1024>& history_;

    // Track processed (client_tick, dummy_idx) pairs for duplicate rejection.
    // Uses a hash of (tick << 16) | (dummy_idx & 0xFFFF).
    std::unordered_set<ae::u32> processed_hits_;
};

} // namespace ahamkara::game

#pragma once

#include "ae/core/types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace ae {

/**
 * @brief Fixed-capacity circular history buffer for server-authoritative
 *        state snapshots.
 *
 * The server records one entry per tick.  Clients (or the server itself
 * during rewind-and-replay) can query the buffer by tick index to retrieve
 * the state that was authoritative at that moment.
 *
 * @tparam State  The per-tick state struct.  Must be trivially copyable.
 *                Typically contains player positions, velocities, health,
 *                and any other fast-changing replicated state.
 * @tparam Capacity  Maximum number of tick entries retained.
 */
template <typename State, usize Capacity>
class ServerHistoryBuffer {
    static_assert(std::is_trivially_copyable_v<State>,
                  "State type must be trivially copyable for efficient snapshots.");

public:
    ServerHistoryBuffer() = default;

    /**
     * @brief Record a new state for the given server tick.
     *
     * If the tick jumps forward by more than one, the intermediate ticks
     * are filled with a copy of the previous state (no interpolation).
     * If the buffer wraps, the oldest entry is silently overwritten.
     */
    void record(u32 server_tick, const State& state) {
        if (count_ > 0 && server_tick > newest_tick_ + 1) {
            // Fill gap: copy the most recent state forward.
            const u32 gap = server_tick - newest_tick_ - 1;
            State prev = entries_[wrap(newest_tick_)];
            for (u32 t = newest_tick_ + 1; t < server_tick; ++t) {
                write_entry(t, prev);
            }
        }

        write_entry(server_tick, state);
    }

    /**
     * @brief Retrieve the state for a given tick, if available.
     *
     * @param tick        The server tick to query.
     * @param out_state   Receives the state.
     * @return true if the tick is within the retained window.
     */
    [[nodiscard]] bool get(u32 tick, State& out_state) const {
        if (count_ == 0) {
            return false;
        }

        const u32 oldest = newest_tick_ >= count_ - 1
                         ? newest_tick_ - static_cast<u32>(count_ - 1)
                         : 0;

        if (tick < oldest || tick > newest_tick_) {
            return false;
        }

        out_state = entries_[wrap(tick)];
        return true;
    }

    /**
     * @brief Get the most recent recorded tick.
     */
    [[nodiscard]] u32 newest_tick() const { return newest_tick_; }

    /**
     * @brief Get the oldest tick still retained in the buffer.
     *        Returns 0 if the buffer is empty.
     */
    [[nodiscard]] u32 oldest_tick() const {
        if (count_ == 0) {
            return 0;
        }
        return newest_tick_ >= count_ - 1
             ? newest_tick_ - static_cast<u32>(count_ - 1)
             : 0;
    }

    /**
     * @brief Number of entries currently stored.
     */
    [[nodiscard]] usize size() const { return count_; }

    /**
     * @brief Maximum capacity.
     */
    [[nodiscard]] static constexpr usize capacity() { return Capacity; }

    /** Clear all entries. */
    void reset() {
        count_ = 0;
        newest_tick_ = 0;
    }

private:
    [[nodiscard]] usize wrap(u32 tick) const {
        return static_cast<usize>(tick % static_cast<u32>(Capacity));
    }

    void write_entry(u32 tick, const State& state) {
        entries_[wrap(tick)] = state;
        newest_tick_ = tick;

        if (count_ < Capacity) {
            ++count_;
        }
    }

    std::array<State, Capacity> entries_ {};
    u32 newest_tick_ {0};
    usize count_ {0};
};

}  // namespace ae

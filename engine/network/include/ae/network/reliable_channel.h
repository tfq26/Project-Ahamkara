#pragma once

#include "ae/core/types.h"

#include <cstddef>
#include <map>
#include <vector>

namespace ae {

/**
 * @brief Reliable-delivery bookkeeping on top of the packet envelope.
 *
 * Buffers reliable outgoing packets keyed by their 16-bit sequence number,
 * consumes the peer's ACK metadata to drop delivered packets, and reports
 * packets that should be retransmitted after a timeout.
 *
 * Transport-agnostic and clock-injected (callers pass `now` and `timeout`), so
 * it is fully unit-testable without a socket. Sequence numbers come from
 * `SequenceTracker::prepare_outgoing()`; the ACK encoding matches
 * `SequenceTracker`: bit `i` of `ack_bitfield` acknowledges sequence
 * `ack_sequence - 1 - i` (16-bit wraparound).
 *
 * This is intentionally NOT wired into the live client/server loops here — that
 * is a separate integration step.
 */
class ReliableChannel {
public:
    struct InFlight {
        std::vector<u8> payload;
        double last_send_time {0.0};
        u32 send_count {1};
    };

    /// Register a reliable packet as in-flight (caller already allocated `seq`).
    void on_send(u16 sequence, const u8* data, std::size_t len, double now) {
        InFlight f;
        if (data != nullptr && len > 0) {
            f.payload.assign(data, data + len);
        }
        f.last_send_time = now;
        f.send_count = 1;
        in_flight_[sequence] = std::move(f);
    }

    /// Consume the peer's ACK state, removing acknowledged packets. `ack_bitfield`
    /// bit `i` acks `ack_sequence - 1 - i` (matches SequenceTracker encoding).
    void on_ack(u16 ack_sequence, u32 ack_bitfield) {
        in_flight_.erase(ack_sequence);
        for (u32 i = 0; i < 32; ++i) {
            if ((ack_bitfield & (1u << i)) != 0u) {
                in_flight_.erase(static_cast<u16>(ack_sequence - 1u - i));
            }
        }
    }

    /// Sequences still unacked whose last send is older than `timeout`. Their
    /// `last_send_time` is refreshed and `send_count` incremented (assumes the
    /// caller retransmits them now). Deterministic (ascending sequence order).
    [[nodiscard]] std::vector<u16> collect_retransmits(double now, double timeout) {
        std::vector<u16> due;
        for (auto& entry : in_flight_) {
            if (now - entry.second.last_send_time >= timeout) {
                due.push_back(entry.first);
                entry.second.last_send_time = now;
                ++entry.second.send_count;
            }
        }
        return due;
    }

    /// Buffered payload for a sequence, or nullptr if not in flight.
    [[nodiscard]] const std::vector<u8>* payload(u16 sequence) const {
        auto it = in_flight_.find(sequence);
        return (it == in_flight_.end()) ? nullptr : &it->second.payload;
    }

    /// How many times a sequence has been sent (1 = original). 0 if not tracked.
    [[nodiscard]] u32 send_count(u16 sequence) const {
        auto it = in_flight_.find(sequence);
        return (it == in_flight_.end()) ? 0u : it->second.send_count;
    }

    [[nodiscard]] std::size_t pending_count() const { return in_flight_.size(); }
    void clear() { in_flight_.clear(); }

private:
    std::map<u16, InFlight> in_flight_;
};

}  // namespace ae

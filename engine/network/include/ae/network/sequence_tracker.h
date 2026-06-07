#pragma once

#include "ae/core/types.h"
#include "ae/network/packet_envelope.h"

namespace ae {

/**
 * @brief Per-peer packet sequence bookkeeping.
 *
 * One instance per connected peer (client has one for the server;
 * server has one per client).  Tracks outgoing sequence numbers
 * and incoming sequence/ACK state so every packet can carry
 * reliability metadata.
 *
 * Sequence numbers are 16-bit and wrap naturally.  Receivers use
 * unsigned subtraction to compute relative ordering; this is safe
 * as long as no more than 32767 packets are in-flight.
 *
 * This tracker does NOT perform retransmission or buffering —
 * those belong to higher-level reliable-channel implementations.
 */
class SequenceTracker {
public:
    SequenceTracker() = default;

    /**
     * @brief Prepare envelope metadata for the next outgoing packet.
     *
     * Advances the outgoing sequence counter and returns an envelope
     * containing the new sequence number and the latest ACK state.
     */
    [[nodiscard]] PacketEnvelope prepare_outgoing();

    /**
     * @brief Process the envelope from a received packet.
     *
     * Updates the incoming ACK state so subsequent outgoing packets
     * will acknowledge this and any previously-missing packets.
     */
    void process_incoming(const PacketEnvelope& envelope);

    /// Total outgoing packets sent through prepare_outgoing().
    [[nodiscard]] u32 packets_sent() const { return sent_count_; }

    /// Total incoming packets processed through process_incoming().
    [[nodiscard]] u32 packets_received() const { return received_count_; }

    /// Estimated lost packets based on sequence gaps (best-effort).
    [[nodiscard]] u32 estimated_lost() const;

private:
    /// Advance the bitfield to acknowledge a newly received sequence.
    void ack_sequence(u16 sequence);

    u16 next_outgoing_ {0};
    u16 latest_received_ {0};
    u32 ack_bitfield_ {0};   // bit i covers (latest_received_ - 1 - i)
    u32 sent_count_ {0};
    u32 received_count_ {0};
    u32 lost_count_ {0};
    bool have_first_ {false}; // true after first incoming packet
};

}  // namespace ae

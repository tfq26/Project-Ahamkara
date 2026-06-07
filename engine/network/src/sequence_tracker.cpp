#include "ae/network/sequence_tracker.h"

namespace ae {

PacketEnvelope SequenceTracker::prepare_outgoing() {
    PacketEnvelope env {};
    env.sequence = next_outgoing_++;
    env.ack_sequence = latest_received_;
    env.ack_bitfield = ack_bitfield_;
    ++sent_count_;
    return env;
}

void SequenceTracker::process_incoming(const PacketEnvelope& envelope) {
    const u16 seq = envelope.sequence;
    ++received_count_;

    if (!have_first_) {
        latest_received_ = seq;
        ack_bitfield_ = 0;
        have_first_ = true;
        return;
    }

    ack_sequence(seq);
}

void SequenceTracker::ack_sequence(u16 sequence) {
    // Unsigned subtraction yields the relative distance even across
    // 16-bit wrap, as long as the distance is ≤ 32767.
    const u16 delta = sequence - latest_received_;

    if (delta == 0) {
        return; // duplicate
    }

    if (delta <= 32768) {
        // sequence is newer than latest_received_.
        const u32 gap = delta - 1;
        lost_count_ += gap;

        // Shift existing bitfield right: old bit i → new bit (i + delta).
        if (delta >= 32) {
            ack_bitfield_ = 0;
        } else {
            ack_bitfield_ <<= delta;
        }

        // Set the bit for the old latest_received_ (now at offset delta-1).
        if (delta - 1 < 32) {
            ack_bitfield_ |= (1U << (delta - 1));
        }

        latest_received_ = sequence;
    } else {
        // sequence is older (out-of-order arrival).
        // How far behind is it?
        const u16 behind = static_cast<u16>(65536U - static_cast<u32>(delta));
        if (behind >= 1 && behind <= 32) {
            const u32 bit_index = behind - 1;
            // If this slot was previously empty, we just filled a gap.
            if ((ack_bitfield_ & (1U << bit_index)) == 0 && lost_count_ > 0) {
                --lost_count_;
            }
            ack_bitfield_ |= (1U << bit_index);
        }
    }
}

u32 SequenceTracker::estimated_lost() const {
    return lost_count_;
}

}  // namespace ae

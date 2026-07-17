#include "wish/types.h"
#include "wish/log.h"

namespace wish {

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
        log_info("SequenceTracker initialized with seq=" + std::to_string(seq));
        return;
    }

    ack_sequence(seq);
}

void SequenceTracker::ack_sequence(u16 sequence) {
    const u16 delta = sequence - latest_received_;

    if (delta == 0) {
        // duplicate — silently ignore
        return;
    }

    if (delta <= 32768) {
        const u32 gap = delta - 1;
        if (gap > 0) {
            log_warning("SequenceTracker: gap of " + std::to_string(gap) +
                        " packets (seq=" + std::to_string(sequence) + ")");
        }
        lost_count_ += gap;

        if (delta >= 32) {
            ack_bitfield_ = 0;
        } else {
            ack_bitfield_ <<= delta;
        }

        if (delta - 1 < 32) {
            ack_bitfield_ |= (1U << (delta - 1));
        }

        latest_received_ = sequence;
    } else {
        // out-of-order arrival
        const u16 behind = static_cast<u16>(65536U - static_cast<u32>(delta));
        if (behind >= 1 && behind <= 32) {
            const u32 bit_index = behind - 1;
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

} // namespace wish

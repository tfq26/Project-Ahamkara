#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace wish {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;

// ---------------------------------------------------------------------------
// NetAddress
// ---------------------------------------------------------------------------
struct NetAddress {
    std::string ip {};
    u16 port {0};

    bool operator==(const NetAddress& other) const {
        return ip == other.ip && port == other.port;
    }

    bool operator!=(const NetAddress& other) const {
        return !(*this == other);
    }
};

// ---------------------------------------------------------------------------
// PacketEnvelope — per-packet reliability metadata
// ---------------------------------------------------------------------------
struct PacketEnvelope {
    u16 sequence {0};
    u16 ack_sequence {0};
    u32 ack_bitfield {0};
};

// ---------------------------------------------------------------------------
// SequenceTracker — per-peer packet sequence bookkeeping
// ---------------------------------------------------------------------------
class SequenceTracker {
  public:
    SequenceTracker() = default;

    [[nodiscard]] PacketEnvelope prepare_outgoing();
    void process_incoming(const PacketEnvelope& envelope);

    [[nodiscard]] u32 packets_sent() const {
        return sent_count_;
    }
    [[nodiscard]] u32 packets_received() const {
        return received_count_;
    }
    [[nodiscard]] u32 estimated_lost() const;

  private:
    void ack_sequence(u16 sequence);

    u16 next_outgoing_ {0};
    u16 latest_received_ {0};
    u32 ack_bitfield_ {0};
    u32 sent_count_ {0};
    u32 received_count_ {0};
    u32 lost_count_ {0};
    bool have_first_ {false};
};

// ---------------------------------------------------------------------------
// Span
// ---------------------------------------------------------------------------
template <typename T>
using Span = std::span<T>;

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------
template <typename T, typename E = std::string>
struct Result {
    T value {};
    E error {};
    bool ok = false;
};

// ---------------------------------------------------------------------------
// trim — standalone whitespace trimming
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::string_view trim(std::string_view sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n'))
        sv.remove_prefix(1);
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n'))
        sv.remove_suffix(1);
    return sv;
}

} // namespace wish

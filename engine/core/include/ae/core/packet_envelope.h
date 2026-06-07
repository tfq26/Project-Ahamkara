#pragma once

#include "ae/core/types.h"

#include <type_traits>

namespace ae {

/**
 * @brief Per-packet reliability metadata carried by every UDP packet.
 *
 * This lives in the core layer so both networking and game packet types can
 * share it without creating a module dependency cycle.
 */
struct PacketEnvelope {
    u16 sequence {0};
    u16 ack_sequence {0};
    u32 ack_bitfield {0};
};

static_assert(std::is_trivially_copyable_v<PacketEnvelope>);

}  // namespace ae

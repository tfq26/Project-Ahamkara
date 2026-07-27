#pragma once

#include <cstdint>

// ── Wish Protocol Version ───────────────────────────────────────────────
//
// The protocol version governs the wire format used by Wish for replication
// frames, session management, and admin commands. Both endpoints (server and
// client/Flashback) must agree on the same PROTOCOL version.
//
// The ABI version covers the C++ binary interface of the wish_engine library.
//
// Policy:
//   PROTOCOL MAJOR — breaking wire format change (field reorder, removed
//                    message type, changed encoding)
//   PROTOCOL MINOR — backward-compatible extension (new optional field,
//                    new message type that can be ignored by old peers)
//   ABI MAJOR      — breaking C++ ABI change (vtable layout, struct padding)
//   ABI MINOR      — additive C++ ABI change (new non-virtual method,
//                    appended struct field with default)
//
// Compatibility:
//   Protocol — same MAJOR, consumer MINOR ≤ producer MINOR
//   ABI      — same MAJOR, consumer MINOR ≤ producer MINOR

namespace wish::core {

struct ProtocolVersion {
    std::uint16_t major;
    std::uint16_t minor;
};

struct WishAbiVersion {
    std::uint16_t major;
    std::uint16_t minor;
};

/// Current protocol version (wire format).
inline constexpr ProtocolVersion kWireProtocol {1, 0};

/// Current Wish ABI version (C++ binary interface).
inline constexpr WishAbiVersion kWishAbiVersion {1, 0};

/// Returns true when @p consumer protocol is compatible with @p producer.
inline constexpr bool is_protocol_compatible(ProtocolVersion consumer,
                                              ProtocolVersion producer) noexcept {
    return consumer.major == producer.major && consumer.minor <= producer.minor;
}

/// Returns true when @p consumer ABI is compatible with @p producer ABI.
inline constexpr bool is_wish_abi_compatible(WishAbiVersion consumer,
                                              WishAbiVersion producer) noexcept {
    return consumer.major == producer.major && consumer.minor <= producer.minor;
}

}  // namespace wish::core

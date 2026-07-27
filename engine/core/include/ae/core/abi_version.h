#pragma once

#include <cstdint>

// ── Ahamkara Engine ABI Version ─────────────────────────────────────────
//
// The ABI version identifies the binary interface contract of the Ahamkara
// engine libraries (ae_core, ae_network, ae_runtime). Flashback consumers
// must link against a compatible ABI level.
//
// Policy:
//   MAJOR — breaking change (struct layout, calling convention, vtable order)
//   MINOR — additive, non-breaking change (new functions, new struct fields
//           with sensible defaults)
//   PATCH — no ABI change; bugfix or internal refactor only
//
// Compatibility: same MAJOR, consumer MINOR ≤ producer MINOR, consumer PATCH ≤ producer PATCH.

namespace ae {

struct AbiVersion {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
};

inline constexpr AbiVersion kEngineAbiVersion {1, 0, 0};

/// Returns true when @p consumer is compatible with @p producer.
inline constexpr bool is_abi_compatible(AbiVersion consumer, AbiVersion producer) noexcept {
    return consumer.major == producer.major &&
           consumer.minor <= producer.minor &&
           consumer.patch <= producer.patch;
}

}  // namespace ae

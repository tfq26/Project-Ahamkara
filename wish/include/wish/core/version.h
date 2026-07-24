#pragma once

// ==========================================================================
// Wish — ABI / Protocol Version
// ==========================================================================
//
// This file defines the ABI version for Wish as well as the wire-protocol
// version used by session / replication frames.  Consumers MUST check
// these constants at compile time against the version they were built
// with to detect accidental mismatches.
//
// Policy
// ------
//   WISH_ABI_VERSION
//       Bumped when any exported symbol signature, class layout, or
//       compile-time constant changes in a way that would cause silent
//       memory corruption or undefined behaviour if a consumer linked
//       against a different version.
//
//   WISH_SESSION_PROTOCOL_VERSION
//       Bumped when the session / replication wire format changes so
//       that two peers with different versions cannot communicate.
//
// Both follow "breaking → bump minor, compatible → keep" within the
// same product major version.

#include <cstdint>

namespace wish::core {

// -----------------------------------------------------------------------
// ABI version  —  stored in the installed package manifest as "abi.level"
// -----------------------------------------------------------------------
inline constexpr std::uint32_t kWishAbiVersion = 1;

// -----------------------------------------------------------------------
// Session / replication wire-protocol version
// -----------------------------------------------------------------------
inline constexpr std::uint32_t kWishSessionProtocolVersion = 1;

// -----------------------------------------------------------------------
// Human-readable helpers
// -----------------------------------------------------------------------
inline constexpr const char* kWishVersionString    = "0.1";
inline constexpr const char* kWishBuildCodename     = "Banshee";

}  // namespace wish::core

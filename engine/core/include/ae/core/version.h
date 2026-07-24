#pragma once

// ==========================================================================
// Ahamkara Engine — ABI / Protocol Version
// ==========================================================================
//
// This file defines the ABI (application-binary interface) version for the
// Ahamkara engine libraries as well as the wire-protocol version for the
// network layer.  Consumers (Flashback, tools, third-party code) MUST
// check these constants at compile time against the version they were
// built with to detect accidental mismatches.
//
// Policy
// ------
//   AE_ABI_VERSION
//       Bumped when any exported symbol signature, class layout, or
//       compile-time constant changes in a way that would cause silent
//       memory corruption or undefined behaviour if a consumer linked
//       against a different version.
//
//   AE_NET_PROTOCOL_VERSION
//       Bumped when the on-wire packet format changes so that two peers
//       with different versions cannot communicate.
//
// Both follow "breaking → bump minor, compatible → keep" within the
// same product major version.

#include <cstdint>

namespace ae::core {

// -----------------------------------------------------------------------
// ABI version  —  stored in the installed package manifest as "abi.level"
// -----------------------------------------------------------------------
inline constexpr std::uint32_t kAhamkaraAbiVersion = 1;

// -----------------------------------------------------------------------
// Network wire-protocol version
// -----------------------------------------------------------------------
inline constexpr std::uint32_t kAhamkaraNetProtocolVersion = 1;

// -----------------------------------------------------------------------
// Human-readable helpers
// -----------------------------------------------------------------------
inline constexpr const char* kAhamkaraVersionString = "0.1.0";
inline constexpr const char* kAhamkaraBuildCodename  = "Axiom";

}  // namespace ae::core

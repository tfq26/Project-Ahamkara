#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

namespace flashback {

/// Flashback error domains (3-character uppercase codes used in FB-* format).
/// Domains match the second segment of the stable code string, e.g. FB-GME-1001.
struct FlashbackDomain {
    static constexpr const char* kGameplay = "GME"; // Gameplay & content
    static constexpr const char* kNetwork = "NET";  // Network & connectivity
    static constexpr const char* kAudio = "AUD";    // Audio subsystem
    static constexpr const char* kBoot = "BOT";     // Boot & initialization
};

/// Stable numeric error codes for Flashback product.
/// Each code has exactly one meaning, owner, message key, and recovery policy.
enum class FlashbackErrorCode : std::uint32_t {
    // --- Gameplay (GME) ---
    kContentIncompatible = 1001,   // Gameplay content version incompatible
    kContentRepairRequired = 1002, // Content corrupt and requires repair

    // --- Network (NET) ---
    kReconnectRequired = 2001, // Network connection lost, needs reconnect

    // --- Audio (AUD) ---
    kMutedAudioFallback = 3001, // Audio device failed, muted fallback active

    // --- Boot (BOT) ---
    kFatalBootFailure = 4001, // Fatal boot/initialization failure

    // Sentinel
    kCount,
};

/// Returns the domain string for a given Flashback error code.
/// Returns nullptr for unknown codes.
[[nodiscard]] constexpr const char* flashback_code_domain(FlashbackErrorCode code) {
    switch (code) {
    case FlashbackErrorCode::kContentIncompatible:
    case FlashbackErrorCode::kContentRepairRequired:
        return FlashbackDomain::kGameplay;
    case FlashbackErrorCode::kReconnectRequired:
        return FlashbackDomain::kNetwork;
    case FlashbackErrorCode::kMutedAudioFallback:
        return FlashbackDomain::kAudio;
    case FlashbackErrorCode::kFatalBootFailure:
        return FlashbackDomain::kBoot;
    default:
        return nullptr;
    }
}

/// Build a stable FB-* code string from a FlashbackErrorCode.
/// Returns empty string if the code is unknown.
/// Buffer must be at least 12 characters ("FB-XXX-XXXX").
constexpr void format_flashback_code(FlashbackErrorCode code, char* buffer, std::size_t buffer_size) {
    if (!buffer || buffer_size < 12) {
        return;
    }
    const char* domain = flashback_code_domain(code);
    if (!domain) {
        buffer[0] = '\0';
        return;
    }
    const auto num = static_cast<std::uint32_t>(code);
    buffer[0] = 'F';
    buffer[1] = 'B';
    buffer[2] = '-';
    buffer[3] = domain[0];
    buffer[4] = domain[1];
    buffer[5] = domain[2];
    buffer[6] = '-';
    buffer[7] = static_cast<char>('0' + (num / 1000) % 10);
    buffer[8] = static_cast<char>('0' + (num / 100) % 10);
    buffer[9] = static_cast<char>('0' + (num / 10) % 10);
    buffer[10] = static_cast<char>('0' + num % 10);
    buffer[11] = '\0';
}

/// Stream a FlashbackErrorCode as its numeric value.
inline std::ostream& operator<<(std::ostream& os, FlashbackErrorCode code) {
    return os << static_cast<std::uint32_t>(code);
}

} // namespace flashback

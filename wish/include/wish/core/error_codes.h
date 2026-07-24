#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>

namespace wish {

/// Wish error domains (4-character uppercase codes used in WS-* format).
/// Domains match the second segment of the stable code string, e.g. WS-AUT-1001.
struct WishDomain {
    static constexpr const char* kAuth = "AUT";     // Authentication & admission
    static constexpr const char* kSession = "SES";  // Session & activity lifecycle
    static constexpr const char* kCapacity = "CAP"; // Capacity & rate limiting
    static constexpr const char* kProtocol = "PRO"; // Protocol & versioning
    static constexpr const char* kBackend = "BAK";  // Backend availability
    static constexpr const char* kPersist = "PER"; // Persistence/storage
    static constexpr const char* kInternal = "INT"; // Internal/system
};

/// Stable numeric error codes for the Wish platform.
/// Each code has exactly one meaning, owner, message key, and recovery policy.
enum class WishErrorCode : std::uint32_t {
    // --- Authentication (AUT) ---
    kAuthRejected = 1001,             // Authentication rejected
    kSessionAdmissionRejected = 1002, // Session admission rejected

    // --- Session & Activity (SES) ---
    kSessionExpired = 2001,      // Session expired
    kActivityUnavailable = 2002, // Activity unavailable

    // --- Capacity (CAP) ---
    kCapacityExceeded = 3001, // Capacity / rate limit exceeded

    // --- Protocol (PRO) ---
    kProtocolError = 4001,           // General protocol error
    kProtocolVersionMismatch = 4002, // Protocol version mismatch

    // --- Backend (BAK) ---
    kBackendUnavailable = 5001, // Backend service unavailable
    kBackendTimeout = 5002,     // Backend request timeout

    // --- Persistence (PER) ---
    kPersistenceNotFound = 6001,         // Document not found
    kPersistenceConflict = 6002,         // Version conflict (stale version)
    kPersistencePermissionDenied = 6003, // Operation not permitted
    kPersistenceQuotaExceeded = 6004,    // Storage quota exceeded
    kPersistencePayloadTooLarge = 6005,  // Single document exceeds size limit
    kPersistenceMalformedPayload = 6006, // Invalid/corrupt document data
    kPersistenceBackendUnavailable = 6007, // Storage backend unavailable

    // --- Internal (INT) ---
    kInternalError = 9001, // Internal/system error

    // Sentinel
    kCount,
};

/// Returns the domain string for a given error code.
/// Returns empty string_view for unknown codes.
[[nodiscard]] constexpr const char* wish_code_domain(WishErrorCode code) {
    switch (code) {
    case WishErrorCode::kAuthRejected:
    case WishErrorCode::kSessionAdmissionRejected:
        return WishDomain::kAuth;
    case WishErrorCode::kSessionExpired:
    case WishErrorCode::kActivityUnavailable:
        return WishDomain::kSession;
    case WishErrorCode::kCapacityExceeded:
        return WishDomain::kCapacity;
    case WishErrorCode::kProtocolError:
    case WishErrorCode::kProtocolVersionMismatch:
        return WishDomain::kProtocol;
    case WishErrorCode::kBackendUnavailable:
    case WishErrorCode::kBackendTimeout:
        return WishDomain::kBackend;
    case WishErrorCode::kPersistenceNotFound:
    case WishErrorCode::kPersistenceConflict:
    case WishErrorCode::kPersistencePermissionDenied:
    case WishErrorCode::kPersistenceQuotaExceeded:
    case WishErrorCode::kPersistencePayloadTooLarge:
    case WishErrorCode::kPersistenceMalformedPayload:
    case WishErrorCode::kPersistenceBackendUnavailable:
        return WishDomain::kPersist;
    case WishErrorCode::kInternalError:
        return WishDomain::kInternal;
    default:
        return nullptr;
    }
}

/// Build a stable WS-* code string from a WishErrorCode.
/// Returns empty string_view if the code is unknown.
/// Buffer must be at least 12 characters ("WS-XXX-XXXX").
constexpr void format_wish_code(WishErrorCode code, char* buffer, std::size_t buffer_size) {
    if (!buffer || buffer_size < 12) {
        return;
    }
    const char* domain = wish_code_domain(code);
    if (!domain) {
        buffer[0] = '\0';
        return;
    }
    const auto num = static_cast<std::uint32_t>(code);
    buffer[0] = 'W';
    buffer[1] = 'S';
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

/// Stream a WishErrorCode as its numeric value.
inline std::ostream& operator<<(std::ostream& os, WishErrorCode code) {
    return os << static_cast<std::uint32_t>(code);
}

} // namespace wish

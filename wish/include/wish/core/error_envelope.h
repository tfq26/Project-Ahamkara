#pragma once

#include "wish/core/error_codes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wish {

/// Maximum length of a WS-* code string: "WS-XXX-XXXX\0" = 12.
inline constexpr std::size_t kMaxCodeString = 12;

/// Maximum length of an incident ID string.
inline constexpr std::size_t kMaxIncidentId = 17;

/// Maximum length of a message key string.
inline constexpr std::size_t kMaxMessageKey = 64;

/// Current wire envelope format version.
inline constexpr std::uint32_t kErrorEnvelopeVersion = 1;

/// Versioned safe wire envelope for Wish error responses.
///
/// Designed for cross-process / cross-machine transport. Contains only
/// stable, public-safe fields. Tokens, credentials, account data, private
/// addresses, and raw backend responses NEVER enter this envelope.
struct ErrorEnvelope {
    /// Wire format version. Must be kErrorEnvelopeVersion for current format.
    std::uint32_t version {kErrorEnvelopeVersion};

    /// Stable Wish error code (numeric, see WishErrorCode).
    std::uint32_t error_code {0};

    /// Human-readable incident ID for support correlation (e.g. "7F4A19C2").
    /// Joins client-visible responses to server logs without becoming a
    /// high-cardinality metric label.
    std::string incident_id {};

    /// Localization key for client-side message lookup (e.g. "errors.auth.rejected").
    std::string message_key {};

    /// Whether the operation may be retried after retry_after_seconds.
    bool retryable {false};

    /// Optional minimum seconds to wait before retrying.
    /// Only meaningful when retryable is true.
    std::uint32_t retry_after_seconds {0};

    /// Returns true if the envelope contains a valid error code.
    [[nodiscard]] bool has_error() const {
        return error_code != 0;
    }

    /// Returns true if the envelope is well-formed and can be safely deserialized.
    [[nodiscard]] bool valid() const;
};

/// Serialize an ErrorEnvelope to a compact text representation.
/// Returns the serialized string, or empty string on error.
[[nodiscard]] std::string serialize_envelope(const ErrorEnvelope& envelope);

/// Deserialize an ErrorEnvelope from a compact text representation.
/// Returns std::nullopt on malformed or unknown data.
[[nodiscard]] std::optional<ErrorEnvelope> deserialize_envelope(std::string_view data);

} // namespace wish

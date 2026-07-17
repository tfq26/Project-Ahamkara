#pragma once

#include "flashback/flashback_error_codes.h"
#include "flashback/flashback_error_catalog.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace flashback {

/// Maximum length of a formatted FB-* code string.
inline constexpr std::size_t kMaxFlashbackCodeString = 12;

/// Maximum length of an incident ID string.
inline constexpr std::size_t kMaxIncidentId = 17;

/// Maximum length of a message key string.
inline constexpr std::size_t kMaxMessageKey = 64;

/// Redacted safe context: never contains paths, tokens, IPs, or stack traces.
struct FlashbackErrorContext {
    std::string external_code {};     // Original external code (AE-* or WS-*) if applicable
    std::string incident_id {};       // Incident ID for support correlation
    std::string message_key {};       // Localization key for UI text lookup
    bool retryable {false};           // Whether the operation can be retried
    std::uint32_t retry_after_ms {0}; // Suggested wait before retry (ms)
};

/// Presentation result for the Flashback error UI.
/// Contains only user-safe fields. No paths, tokens, IPs, or stack traces.
struct FlashbackErrorPresentation {
    std::string code_string {};  // e.g. "FB-GME-1002"
    std::string incident_id {};  // e.g. "7F4A-19C2"
    std::string message_key {};  // Localization key for UI
    std::string title {};        // Default title (non-localized fallback)
    std::string action_label {}; // Action button label
    FlashbackRecoveryAction recovery {FlashbackRecoveryAction::None};
    std::string support_slug {}; // Support page slug
    bool user_visible {true};
};

/// Build a presentation for a Flashback-owned error code.
/// Shows one actionable top-level FB-* code and incident ID.
[[nodiscard]] FlashbackErrorPresentation present_flashback_error(
    FlashbackErrorCode code,
    std::optional<std::string_view> incident_id = std::nullopt);

/// Build a presentation for an external error (AE-* or WS-*).
/// Maps the external code to the appropriate Flashback presentation strategy.
/// Unknown external codes degrade to safe generic presentation.
[[nodiscard]] FlashbackErrorPresentation present_external_error(
    std::string_view product,
    std::string_view domain,
    std::optional<std::string_view> incident_id = std::nullopt);

/// Build a presentation from a WS-* error envelope context.
/// Uses the message_key from the context if available, otherwise maps via domain.
[[nodiscard]] FlashbackErrorPresentation present_from_context(
    const FlashbackErrorContext& context);

/// Redact potentially sensitive values before display or persistence.
/// Removes paths, tokens, IP addresses, and stack-trace-like patterns.
[[nodiscard]] std::string redact_value(std::string_view value);

/// Get the action label for a recovery action.
[[nodiscard]] std::string_view recovery_action_label(FlashbackRecoveryAction action);

/// Check if a recovery action is destructive (e.g. repair, update, fatal).
/// Service-side errors should check retry guidance before prescribing destructive changes.
[[nodiscard]] bool is_destructive_recovery(FlashbackRecoveryAction action);

} // namespace flashback

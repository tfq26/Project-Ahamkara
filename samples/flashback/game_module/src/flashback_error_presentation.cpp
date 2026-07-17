#include "flashback/flashback_error_presentation.h"
#include "flashback/flashback_error_catalog.h"
#include "flashback/flashback_error_codes.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace flashback {
namespace {

/// Generate a compact incident ID from a simple counter or hash.
std::string generate_incident_id() {
    static std::uint64_t counter = 0;
    ++counter;
    char buf[17];
    const auto high = static_cast<std::uint32_t>((counter >> 32) & 0xFFFFu);
    const auto low = static_cast<std::uint32_t>(counter & 0xFFFFu);
    std::snprintf(buf, sizeof(buf), "%04X-%04X", high, low);
    return std::string(buf);
}

/// Format a FB-* code string from a FlashbackErrorCode.
std::string format_code_string(FlashbackErrorCode code) {
    char buf[kMaxFlashbackCodeString];
    format_flashback_code(code, buf, sizeof(buf));
    return std::string(buf);
}

/// Get the default title text for a Flashback error code.
const char* default_title(FlashbackErrorCode code) {
    const auto& catalog = FlashbackErrorCatalog::instance();
    const auto* entry = catalog.find(code);
    if (entry) {
        return entry->title;
    }
    return "An unexpected error occurred";
}

/// Get the support slug for a Flashback error code.
const char* support_slug_for_code(FlashbackErrorCode code) {
    const auto& catalog = FlashbackErrorCatalog::instance();
    const auto* entry = catalog.find(code);
    if (entry) {
        return entry->support_slug;
    }
    return "fb-unknown";
}

/// Get message key for a Flashback error code.
const char* message_key_for_code(FlashbackErrorCode code) {
    const auto& catalog = FlashbackErrorCatalog::instance();
    const auto* entry = catalog.find(code);
    if (entry) {
        return entry->message_key;
    }
    return "errors.generic.unknown";
}

} // anonymous namespace

FlashbackErrorPresentation present_flashback_error(
    FlashbackErrorCode code,
    std::optional<std::string_view> incident_id) {

    FlashbackErrorPresentation result;
    result.code_string = format_code_string(code);
    result.incident_id = incident_id ? std::string(*incident_id) : generate_incident_id();
    result.message_key = message_key_for_code(code);
    result.title = default_title(code);
    result.support_slug = support_slug_for_code(code);

    const auto& catalog = FlashbackErrorCatalog::instance();
    const auto* entry = catalog.find(code);
    if (entry) {
        result.recovery = entry->recovery;
        result.user_visible = entry->user_visible;
        result.action_label = std::string(recovery_action_label(entry->recovery));
    } else {
        result.recovery = FlashbackRecoveryAction::RetryNow;
        result.user_visible = true;
        result.action_label = std::string(recovery_action_label(FlashbackRecoveryAction::RetryNow));
    }

    return result;
}

FlashbackErrorPresentation present_external_error(
    std::string_view product,
    std::string_view domain,
    std::optional<std::string_view> incident_id) {

    // Generate incident ID if not provided
    const std::string id = incident_id ? std::string(*incident_id) : generate_incident_id();

    // Map external code to Flashback presentation strategy.
    // Service-side errors must check retry guidance before prescribing
    // destructive changes — that check happens in the recovery layer.
    const FlashbackErrorCode fb_code = map_external_to_flashback_code(product, domain);

    // Build presentation from the mapped FB code
    auto result = present_flashback_error(fb_code, id);

    // Preserve the original external code as context
    if (!result.incident_id.empty() && result.incident_id == id) {
        // Keep incident ID
    }

    return result;
}

FlashbackErrorPresentation present_from_context(
    const FlashbackErrorContext& context) {

    FlashbackErrorPresentation result;

    // Parse the external code string to extract product and domain
    result.incident_id = context.incident_id.empty()
                             ? generate_incident_id()
                             : context.incident_id;

    if (!context.external_code.empty()) {
        // Extract product and domain from code string like "AE-NET-1004" or "WS-AUT-1002"
        const auto dash1 = context.external_code.find('-');
        const auto dash2 = context.external_code.find('-', dash1 + 1);

        if (dash1 != std::string::npos && dash2 != std::string::npos) {
            const std::string_view product(
                context.external_code.data(), dash1);
            const std::string_view domain(
                context.external_code.data() + dash1 + 1, dash2 - dash1 - 1);

            const auto fb_code = map_external_to_flashback_code(product, domain);
            auto mapped = present_flashback_error(fb_code, result.incident_id);

            // Override with context-provided message_key if available
            if (!context.message_key.empty()) {
                mapped.message_key = context.message_key;
            }

            result = std::move(mapped);
        } else {
            // Unparseable code string -> generic
            auto generic = present_flashback_error(
                FlashbackErrorCode::kContentRepairRequired, result.incident_id);
            result = std::move(generic);
        }
    } else {
        // No external code -> generic
        auto generic = present_flashback_error(
            FlashbackErrorCode::kContentRepairRequired, result.incident_id);
        result = std::move(generic);
    }

    // Apply context retry metadata.
    // Service-side errors that would prescribe destructive changes should
    // defer to retry when the envelope says retryable.
    if (context.retryable) {
        const bool is_destructive = is_destructive_recovery(result.recovery);
        const bool is_none = (result.recovery == FlashbackRecoveryAction::None);
        if (is_destructive || is_none) {
            result.recovery = FlashbackRecoveryAction::RetryNow;
            result.action_label = std::string(recovery_action_label(FlashbackRecoveryAction::RetryNow));
        }
        // Otherwise keep the existing non-destructive recovery (e.g. Reconnect)
    }

    return result;
}

std::string redact_value(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    // Redact paths: anything starting with /, ~/, ./ or containing ://
    if (!value.empty() &&
        (value[0] == '/' || value[0] == '~' ||
         (value.size() >= 2 && value[0] == '.' && value[1] == '/') ||
         value.find("://") != std::string_view::npos)) {
        return "[REDACTED_PATH]";
    }

    // Redact stack-trace-like patterns (e.g. " at " or "0x" hex addresses)
    if (value.find("0x") != std::string_view::npos ||
        value.find(" at ") != std::string_view::npos ||
        value.find("::") != std::string_view::npos && value.find(")") != std::string_view::npos) {
        return "[REDACTED_TRACE]";
    }

    // Redact tokens (alphanumeric strings >= 32 chars that aren't incident IDs)
    if (value.size() >= 32 && value.size() <= 64) {
        bool all_alnum = true;
        for (char c : value) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                all_alnum = false;
                break;
            }
        }
        if (all_alnum) {
            return "[REDACTED_TOKEN]";
        }
    }

    // Check for IP addresses (simple pattern)
    int dot_count = 0;
    bool looks_like_ip = true;
    for (char c : value) {
        if (c == '.')
            ++dot_count;
        else if (c < '0' || c > '9') {
            looks_like_ip = false;
            break;
        }
    }
    if (dot_count == 3 && looks_like_ip && value.size() >= 7) {
        return "[REDACTED_IP]";
    }

    return std::string(value);
}

std::string_view recovery_action_label(FlashbackRecoveryAction action) {
    switch (action) {
    case FlashbackRecoveryAction::None:
        return "Close";
    case FlashbackRecoveryAction::RetryNow:
        return "Retry";
    case FlashbackRecoveryAction::Reconnect:
        return "Reconnect";
    case FlashbackRecoveryAction::RepairContent:
        return "Repair Content";
    case FlashbackRecoveryAction::UpdateRequired:
        return "Update Required";
    case FlashbackRecoveryAction::MutedFallback:
        return "Continue with muted audio";
    case FlashbackRecoveryAction::FatalShutdown:
        return "Close Flashback";
    default:
        return "Close";
    }
}

bool is_destructive_recovery(FlashbackRecoveryAction action) {
    switch (action) {
    case FlashbackRecoveryAction::RepairContent:
    case FlashbackRecoveryAction::UpdateRequired:
    case FlashbackRecoveryAction::FatalShutdown:
        return true;
    case FlashbackRecoveryAction::None:
    case FlashbackRecoveryAction::RetryNow:
    case FlashbackRecoveryAction::Reconnect:
    case FlashbackRecoveryAction::MutedFallback:
        return false;
    default:
        return false;
    }
}

} // namespace flashback

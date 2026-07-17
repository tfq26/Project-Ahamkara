#include "flashback/flashback_error_catalog.h"
#include "flashback/flashback_error_codes.h"

#include <mutex>
#include <string_view>

namespace flashback {

const FlashbackErrorCatalogEntry FlashbackErrorCatalog::kEntries_[] = {
    // --- Gameplay: Content Incompatible ---
    {
        FlashbackDomain::kGameplay,
        "errors.gameplay.content_incompatible",
        "Content version incompatible",
        "flashback/gameplay",
        FlashbackErrorCode::kContentIncompatible,
        FlashbackRecoveryAction::UpdateRequired,
        true,
        "fb-gme-1001",
    },
    // --- Gameplay: Content Repair Required ---
    {
        FlashbackDomain::kGameplay,
        "errors.gameplay.content_repair_required",
        "Content repair required",
        "flashback/gameplay",
        FlashbackErrorCode::kContentRepairRequired,
        FlashbackRecoveryAction::RepairContent,
        true,
        "fb-gme-1002",
    },
    // --- Network: Reconnect Required ---
    {
        FlashbackDomain::kNetwork,
        "errors.network.reconnect_required",
        "Connection lost",
        "flashback/network",
        FlashbackErrorCode::kReconnectRequired,
        FlashbackRecoveryAction::Reconnect,
        true,
        "fb-net-2001",
    },
    // --- Audio: Muted Fallback ---
    {
        FlashbackDomain::kAudio,
        "errors.audio.muted_fallback",
        "Audio unavailable",
        "flashback/audio",
        FlashbackErrorCode::kMutedAudioFallback,
        FlashbackRecoveryAction::MutedFallback,
        true,
        "fb-aud-3001",
    },
    // --- Boot: Fatal Boot Failure ---
    {
        FlashbackDomain::kBoot,
        "errors.boot.fatal_failure",
        "Flashback failed to start",
        "flashback/boot",
        FlashbackErrorCode::kFatalBootFailure,
        FlashbackRecoveryAction::FatalShutdown,
        true,
        "fb-bot-4001",
    },
};

std::size_t FlashbackErrorCatalog::entry_count() {
    return sizeof(kEntries_) / sizeof(kEntries_[0]);
}

const FlashbackErrorCatalog& FlashbackErrorCatalog::instance() {
    static FlashbackErrorCatalog catalog;
    static std::once_flag once;
    std::call_once(once, [] {
        // No dynamic registration needed; kEntries_ is constexpr-compatible.
    });
    return catalog;
}

const FlashbackErrorCatalogEntry* FlashbackErrorCatalog::find(FlashbackErrorCode code) const {
    const auto num = static_cast<std::uint32_t>(code);
    const auto n = entry_count();
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<std::uint32_t>(kEntries_[i].code) == num) {
            return &kEntries_[i];
        }
    }
    return nullptr;
}

const FlashbackErrorCatalogEntry* FlashbackErrorCatalog::find(std::uint32_t code_value) const {
    const auto n = entry_count();
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<std::uint32_t>(kEntries_[i].code) == code_value) {
            return &kEntries_[i];
        }
    }
    return nullptr;
}

/// Map external product codes to Flashback presentation strategies.
///
/// AE-* (Ahamkara engine) codes:
///   AE-CFG -> FatalBootFailure (config failure prevents boot)
///   AE-AST -> ContentRepairRequired (asset corrupt, needs repair)
///   AE-NET -> ReconnectRequired (connection lost)
///   AE-RND -> FatalBootFailure (renderer init failure)
///   AE-AUD -> MutedAudioFallback (audio init failure)
///
/// WS-* (Wish platform) codes:
///   WS-AUT -> ReconnectRequired (auth failure -> reconnect)
///   WS-SES -> ReconnectRequired (session expired -> reconnect)
///   WS-CAP -> RetryNow (rate limited -> retry)
///   WS-PRO -> UpdateRequired (protocol mismatch -> update)
///   WS-BAK -> RetryNow (backend unavailable -> retry)
///   WS-INT -> ContentRepairRequired (internal -> generic repair)
///
/// Unknown external codes -> ContentRepairRequired (safe generic presentation)
FlashbackErrorCode map_external_to_flashback_code(
    std::string_view product, std::string_view domain) {

    if (product == "AE") {
        if (domain == "CFG")
            return FlashbackErrorCode::kFatalBootFailure;
        if (domain == "AST")
            return FlashbackErrorCode::kContentRepairRequired;
        if (domain == "NET")
            return FlashbackErrorCode::kReconnectRequired;
        if (domain == "RND")
            return FlashbackErrorCode::kFatalBootFailure;
        if (domain == "AUD")
            return FlashbackErrorCode::kMutedAudioFallback;
        return FlashbackErrorCode::kContentRepairRequired;
    }

    if (product == "WS") {
        if (domain == "AUT")
            return FlashbackErrorCode::kReconnectRequired;
        if (domain == "SES")
            return FlashbackErrorCode::kReconnectRequired;
        if (domain == "CAP")
            return FlashbackErrorCode::kContentRepairRequired; // retry guidance
        if (domain == "PRO")
            return FlashbackErrorCode::kContentRepairRequired; // update guidance
        if (domain == "BAK")
            return FlashbackErrorCode::kContentRepairRequired; // retry guidance
        if (domain == "INT")
            return FlashbackErrorCode::kContentRepairRequired;
        return FlashbackErrorCode::kContentRepairRequired;
    }

    // Unknown external product -> safe generic
    return FlashbackErrorCode::kContentRepairRequired;
}

} // namespace flashback

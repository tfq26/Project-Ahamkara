#pragma once

#include "flashback/flashback_error_codes.h"

#include <cstdint>
#include <string_view>

namespace flashback {

/// Recovery policy for a Flashback error code.
enum class FlashbackRecoveryAction : std::uint8_t {
    None = 0,
    RetryNow,
    Reconnect,
    RepairContent,
    UpdateRequired,
    MutedFallback,
    FatalShutdown,
};

/// Catalog entry for a single Flashback error code.
struct FlashbackErrorCatalogEntry {
    const char* domain;
    const char* message_key;
    const char* title;
    const char* owner;
    FlashbackErrorCode code;
    FlashbackRecoveryAction recovery;
    bool user_visible;
    const char* support_slug;
};

/// Immutable catalog of Flashback error descriptors.
/// Provides lookup by FlashbackErrorCode with human-readable metadata.
class FlashbackErrorCatalog {
  public:
    /// Returns the singleton catalog instance.
    static const FlashbackErrorCatalog& instance();

    /// Look up a catalog entry by error code.
    /// Returns nullptr if the code is not registered.
    [[nodiscard]] const FlashbackErrorCatalogEntry* find(FlashbackErrorCode code) const;

    /// Look up a catalog entry by numeric value.
    [[nodiscard]] const FlashbackErrorCatalogEntry* find(std::uint32_t code_value) const;

    /// Returns the total number of registered entries.
    [[nodiscard]] std::size_t size() const {
        return entry_count();
    }

  private:
    FlashbackErrorCatalog() = default;
    static const FlashbackErrorCatalogEntry kEntries_[];
    [[nodiscard]] static std::size_t entry_count();
};

/// Map an external product code (AE-* or WS-*) to the nearest Flashback
/// presentation strategy. This returns the Flashback error that best describes
/// how to present the external error to the user.
///
/// Returns a FlashbackErrorCode appropriate for the external product/domain
/// pair. Returns kContentRepairRequired for unrecognized external codes
/// (safe generic presentation).
[[nodiscard]] FlashbackErrorCode map_external_to_flashback_code(
    std::string_view product,
    std::string_view domain);

} // namespace flashback

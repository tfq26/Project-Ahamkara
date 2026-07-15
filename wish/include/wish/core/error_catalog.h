#pragma once

#include "wish/core/error_codes.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace wish {

/// Recovery policy for a Wish error code.
enum class RecoveryPolicy : std::uint8_t {
    None = 0,
    RetryNow,
    RetryBackoff,
    Reconnect,
    RestartSession,
    ContactSupport,
};

/// Catalog entry for a single Wish error code.
struct ErrorCatalogEntry {
    WishErrorCode code;
    const char* domain;
    const char* message_key;
    const char* title;
    const char* owner;
    RecoveryPolicy recovery;
    bool user_visible;
};

/// Immutable catalog of Wish error descriptors.
/// Provides lookup by WishErrorCode with human-readable metadata.
class ErrorCatalog {
public:
    /// Returns the singleton catalog instance.
    static const ErrorCatalog& instance();

    /// Look up a catalog entry by error code.
    /// Returns nullptr if the code is not registered.
    [[nodiscard]] const ErrorCatalogEntry* find(WishErrorCode code) const;

    /// Look up a catalog entry by numeric value.
    [[nodiscard]] const ErrorCatalogEntry* find(std::uint32_t code_value) const;

    /// Returns the total number of registered entries.
    [[nodiscard]] std::size_t size() const { return entry_count(); }

private:
    ErrorCatalog() = default;
    static const ErrorCatalogEntry kEntries_[];
    [[nodiscard]] static std::size_t entry_count();
};

/// Map a native/backend failure to the closest Wish error code.
/// This is the safe diagnostic mapping layer: raw backend errors are
/// translated to stable WS-* codes without exposing backend response bodies.
///
/// If no mapping is found, returns WishErrorCode::kInternalError.
[[nodiscard]] WishErrorCode map_native_to_wish_code(
    std::string_view native_domain,
    std::int64_t native_code);

} // namespace wish

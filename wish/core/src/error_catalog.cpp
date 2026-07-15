#include "wish/core/error_catalog.h"
#include "wish/core/error_codes.h"

#include <mutex>

namespace wish {

const ErrorCatalogEntry ErrorCatalog::kEntries_[] = {
    // --- Authentication ---
    {
        WishErrorCode::kAuthRejected,
        WishDomain::kAuth,
        "errors.auth.rejected",
        "Authentication rejected",
        "wish/auth",
        RecoveryPolicy::RetryNow,
        true,
    },
    {
        WishErrorCode::kSessionAdmissionRejected,
        WishDomain::kAuth,
        "errors.auth.admission_rejected",
        "Session admission rejected",
        "wish/session",
        RecoveryPolicy::RetryNow,
        true,
    },
    // --- Session & Activity ---
    {
        WishErrorCode::kSessionExpired,
        WishDomain::kSession,
        "errors.session.expired",
        "Session expired",
        "wish/session",
        RecoveryPolicy::Reconnect,
        true,
    },
    {
        WishErrorCode::kActivityUnavailable,
        WishDomain::kSession,
        "errors.session.activity_unavailable",
        "Activity unavailable",
        "wish/session",
        RecoveryPolicy::RetryNow,
        true,
    },
    // --- Capacity ---
    {
        WishErrorCode::kCapacityExceeded,
        WishDomain::kCapacity,
        "errors.capacity.exceeded",
        "Capacity exceeded",
        "wish/admin",
        RecoveryPolicy::RetryBackoff,
        true,
    },
    // --- Protocol ---
    {
        WishErrorCode::kProtocolError,
        WishDomain::kProtocol,
        "errors.protocol.error",
        "Protocol error",
        "wish/net",
        RecoveryPolicy::Reconnect,
        true,
    },
    {
        WishErrorCode::kProtocolVersionMismatch,
        WishDomain::kProtocol,
        "errors.protocol.version_mismatch",
        "Protocol version mismatch",
        "wish/net",
        RecoveryPolicy::RestartSession,
        true,
    },
    // --- Backend ---
    {
        WishErrorCode::kBackendUnavailable,
        WishDomain::kBackend,
        "errors.backend.unavailable",
        "Backend unavailable",
        "wish/integrations",
        RecoveryPolicy::RetryBackoff,
        true,
    },
    {
        WishErrorCode::kBackendTimeout,
        WishDomain::kBackend,
        "errors.backend.timeout",
        "Backend timeout",
        "wish/integrations",
        RecoveryPolicy::RetryNow,
        true,
    },
    // --- Internal ---
    {
        WishErrorCode::kInternalError,
        WishDomain::kInternal,
        "errors.internal.error",
        "Internal system error",
        "wish/core",
        RecoveryPolicy::ContactSupport,
        true,
    },
};

std::size_t ErrorCatalog::entry_count() {
    return sizeof(kEntries_) / sizeof(kEntries_[0]);
}

const ErrorCatalog& ErrorCatalog::instance() {
    static ErrorCatalog catalog;
    static std::once_flag once;
    std::call_once(once, [] {
        // No dynamic registration needed; kEntries_ is constexpr-compatible.
    });
    return catalog;
}

const ErrorCatalogEntry* ErrorCatalog::find(WishErrorCode code) const {
    const auto num = static_cast<std::uint32_t>(code);
    const auto n = entry_count();
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<std::uint32_t>(kEntries_[i].code) == num) {
            return &kEntries_[i];
        }
    }
    return nullptr;
}

const ErrorCatalogEntry* ErrorCatalog::find(std::uint32_t code_value) const {
    const auto n = entry_count();
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<std::uint32_t>(kEntries_[i].code) == code_value) {
            return &kEntries_[i];
        }
    }
    return nullptr;
}

/// Map native/backend domain strings to Wish error codes.
WishErrorCode map_native_to_wish_code(std::string_view native_domain, std::int64_t native_code) {
    // Nakama backend errors
    if (native_domain == "nakama") {
        switch (native_code) {
        case 1:   // Unknown connection error
        case 2:   // Transport error
            return WishErrorCode::kBackendUnavailable;
        case 3:   // Authentication error
            return WishErrorCode::kAuthRejected;
        case 4:   // Session error
            return WishErrorCode::kSessionExpired;
        case 5:   // Capacity exceeded
            return WishErrorCode::kCapacityExceeded;
        default:
            return WishErrorCode::kBackendUnavailable;
        }
    }

    if (native_domain == "nakama_grpc") {
        switch (native_code) {
        case 14:  // UNAVAILABLE
            return WishErrorCode::kBackendUnavailable;
        case 4:   // DEADLINE_EXCEEDED
            return WishErrorCode::kBackendTimeout;
        case 16:  // UNAUTHENTICATED
            return WishErrorCode::kAuthRejected;
        case 10:  // ABORTED
            return WishErrorCode::kCapacityExceeded;
        default:
            return WishErrorCode::kBackendUnavailable;
        }
    }

    // System/network errors
    if (native_domain == "system") {
        switch (native_code) {
        case 60:  // ETIMEDOUT
        case 61:  // ECONNREFUSED
        case 54:  // ECONNRESET
            return WishErrorCode::kBackendUnavailable;
        default:
            return WishErrorCode::kBackendUnavailable;
        }
    }

    return WishErrorCode::kInternalError;
}

} // namespace wish

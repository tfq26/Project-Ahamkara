#include "ae/core/error_registry.h"

#include <mutex>
#include <unordered_set>

namespace ae {
namespace {

const ErrorDescriptor kAhamkaraActive[] = {
    {
        ErrorCode("AE-CFG-0001"),
        "Configuration failure",
        "error.cfg.load_failed",
        "core/config",
        ErrorSeverity::Error,
        RecoveryPolicy{RecoveryAction::RestartApp, 0, 0, true},
        "docs/operations/error-codes.md#ae-cfg-0001",
    },
    {
        ErrorCode("AE-AST-0001"),
        "Asset load failure",
        "error.asset.load_failed",
        "core/assets",
        ErrorSeverity::Error,
        RecoveryPolicy{RecoveryAction::Retry, 2, 250, true},
        "docs/operations/error-codes.md#ae-ast-0001",
    },
    {
        ErrorCode("AE-NET-0001"),
        "Socket failure",
        "error.net.socket_failed",
        "network",
        ErrorSeverity::Error,
        RecoveryPolicy{RecoveryAction::Retry, 3, 500, true},
        "docs/operations/error-codes.md#ae-net-0001",
    },
    {
        ErrorCode("AE-RND-0001"),
        "Renderer init failure",
        "error.render.init_failed",
        "render",
        ErrorSeverity::Fatal,
        RecoveryPolicy{RecoveryAction::RestartApp, 0, 0, true},
        "docs/operations/error-codes.md#ae-rnd-0001",
    },
    {
        ErrorCode("AE-AUD-0001"),
        "Audio init failure",
        "error.audio.init_failed",
        "audio",
        ErrorSeverity::Error,
        RecoveryPolicy{RecoveryAction::RestartSubsystem, 1, 0, true},
        "docs/operations/error-codes.md#ae-aud-0001",
    },
};

} // namespace

ErrorRegistry& ErrorRegistry::instance() {
    static ErrorRegistry registry;
    static std::once_flag once;
    std::call_once(once, [] {
        registry.register_descriptors(ahamkara_active_error_descriptors());
    });
    return registry;
}

bool ErrorRegistry::register_descriptors(std::span<const ErrorDescriptor> descriptors) {
    std::unordered_set<std::string> seen;
    for (const auto& existing : descriptors_) {
        seen.insert(std::string(existing.code.text()));
    }

    std::vector<ErrorDescriptor> pending;
    pending.reserve(descriptors.size());
    for (const auto& d : descriptors) {
        if (!d.code.valid() || !ErrorCode::is_well_formed(d.code.text())) {
            return false;
        }
        const std::string key(d.code.text());
        if (seen.contains(key)) {
            return false;
        }
        seen.insert(key);
        pending.push_back(d);
    }
    descriptors_.insert(descriptors_.end(), pending.begin(), pending.end());
    return true;
}

const ErrorDescriptor* ErrorRegistry::find(const ErrorCode& code) const {
    return find(code.text());
}

const ErrorDescriptor* ErrorRegistry::find(std::string_view code_text) const {
    for (const auto& d : descriptors_) {
        if (d.code.text() == code_text) {
            return &d;
        }
    }
    return nullptr;
}

bool ErrorRegistry::validate() const {
    std::unordered_set<std::string> seen;
    for (const auto& d : descriptors_) {
        if (!ErrorCode::is_well_formed(d.code.text())) {
            return false;
        }
        const std::string key(d.code.text());
        if (seen.contains(key)) {
            return false;
        }
        seen.insert(key);
    }
    return true;
}

std::span<const ErrorDescriptor> ahamkara_active_error_descriptors() {
    return kAhamkaraActive;
}

const ErrorCode& ae_cfg_0001() {
    static const ErrorCode code("AE-CFG-0001");
    return code;
}
const ErrorCode& ae_ast_0001() {
    static const ErrorCode code("AE-AST-0001");
    return code;
}
const ErrorCode& ae_net_0001() {
    static const ErrorCode code("AE-NET-0001");
    return code;
}
const ErrorCode& ae_rnd_0001() {
    static const ErrorCode code("AE-RND-0001");
    return code;
}
const ErrorCode& ae_aud_0001() {
    static const ErrorCode code("AE-AUD-0001");
    return code;
}

} // namespace ae

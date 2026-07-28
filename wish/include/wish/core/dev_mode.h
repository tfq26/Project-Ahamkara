#pragma once

#include <cstdlib>
#include <string_view>

namespace wish::core {

/// Returns true when the process is running in explicit development mode.
///
/// Development mode is activated by setting the environment variable
/// WISH_DEV_MODE=1 (or "true"/"yes").  In development mode authentication
/// may be relaxed and diagnostics may include additional detail.
///
/// In production (dev mode off), the platform fails closed:
/// - Authentication denies all requests by default.
/// - Backend connectivity errors are surfaced as stable WS-* codes without
///   exposing raw backend response bodies.
/// - Incident IDs are generated server-side and never contain PII.
[[nodiscard]] inline bool is_dev_mode() {
    if (const char* raw = std::getenv("WISH_DEV_MODE")) {
        const std::string_view value {raw};
        return value == "1" || value == "true" || value == "yes";
    }
    return false;
}

} // namespace wish::core

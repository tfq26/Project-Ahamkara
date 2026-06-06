#pragma once

#include <cstdint>
#include <string>

namespace ahamkara::client {

/// Client startup configuration loaded from an optional config file.
/// Every field has a sensible default so a missing file degrades gracefully.
struct ClientConfig {
    // --- Video ---------------------------------------------------------------
    std::int32_t window_width  {1280};
    std::int32_t window_height {720};
    bool         fullscreen    {false};
    float        gamma         {1.0F};   // brightness multiplier (0.5–2.0)

    // --- Input ---------------------------------------------------------------
    float mouse_sensitivity {1.0F};

    // --- Network -------------------------------------------------------------
    std::string server_ip {"127.0.0.1"};

    /// Populate fields from the key=value config file at `path`.
    /// Unknown keys are ignored. Invalid values fall back to defaults.
    /// Returns false when the file cannot be opened (treated as "use defaults").
    [[nodiscard]] bool load_from_file(const std::string& path);
};

}  // namespace ahamkara::client

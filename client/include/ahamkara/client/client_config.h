#pragma once

#include <cstdint>
#include <string>

namespace ahamkara::client {

/**
 * @brief Audio-specific configuration loaded from the client config file.
 *
 * Per-category volume is provided for future mixer bus routing.
 * Currently the master and SFX volumes are the primary controls.
 */
struct AudioConfig {
    bool     enabled           {true};
    float    master_volume     {1.0f};   ///< 0.0 = mute, 1.0 = nominal
    float    sfx_volume        {1.0f};   ///< Sound effects (hits, footsteps)
    float    weapon_volume     {1.0f};   ///< Weapon fire and reload
    float    ui_volume         {1.0f};   ///< Menu / HUD sounds
    float    music_volume      {1.0f};   ///< Background music
    float    ambient_volume    {1.0f};   ///< World ambient
};

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

    // --- Audio ---------------------------------------------------------------
    AudioConfig audio {};

    /// Populate fields from the key=value config file at `path`.
    /// Unknown keys are ignored. Invalid values fall back to defaults.
    /// Returns false when the file cannot be opened (treated as "use defaults").
    [[nodiscard]] bool load_from_file(const std::string& path);
    [[nodiscard]] bool save_to_file(const std::string& path) const;
};

}  // namespace ahamkara::client

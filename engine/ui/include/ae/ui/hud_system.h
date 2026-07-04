#pragma once

#include "ae/ui/hud_element.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ae::ui {

/// Manages a collection of HUD elements loaded from a JSON layout file.
/// Supports hot-reload (poll_hot_reload each frame).
class HudSystem {
public:
    /// Load HUD layout from a JSON file. Returns true on success.
    bool load(const std::string& path);

    /// Render all visible HUD elements.
    void render(float screen_w, float screen_h, const HudState& state);

    /// Check for file changes and reload if modified.
    void poll_hot_reload();

    [[nodiscard]] bool loaded() const { return !elements_.empty(); }

private:
    struct HudElementInstance {
        std::unique_ptr<HudElement> element;
        std::string type;
    };

    std::vector<HudElementInstance> elements_;
    std::string path_;
    std::filesystem::file_time_type last_write_;
};

}  // namespace ae::ui

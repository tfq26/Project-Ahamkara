#include "ae/ui/settings_catalog.h"

namespace ae::ui {

const std::vector<UnavailableSetting>& unavailable_settings() {
    static const std::vector<UnavailableSetting> kCatalog = {
        {"vsync", "V-Sync",
         "Not supported by the current renderer."},
        {"show_fps", "Show FPS Counter",
         "Not implemented in this build."},
        {"audio_output_device", "Output Device",
         "Device selection is not supported by the current audio backend."},
        {"hud_enabled", "Show HUD",
         "Not implemented in this build."},
        {"crosshair_enabled", "Show Crosshair",
         "Not implemented in this build."},
        {"minimap_enabled", "Show Minimap",
         "Not implemented in this build."},
        {"hitmarkers", "Hitmarkers",
         "Not implemented in this build."},
        {"damage_numbers", "Damage Numbers",
         "Not implemented in this build."},
        {"input_rebinding", "Input Rebinding",
         "Rebinding is not supported in this build."},
    };
    return kCatalog;
}

} // namespace ae::ui

#pragma once

#include "ae/render/compiled_level.h"

#include <string>
#include <string_view>
#include <vector>

namespace ahamkara::game {

/// Runtime representation of a compiled level asset loaded from a `.aelevel`
/// file.  Owns the decompiled LevelAsset and exposes helper queries so
/// gameplay code never touches the ae::render:: namespace directly.
class Map {
public:
    /// Load a compiled `.aelevel` file from disk.  Returns true on success.
    [[nodiscard]] bool load(const std::string& path);

    /// Create from an already-loaded LevelAsset (e.g. from the asset pipeline).
    [[nodiscard]] static Map from_asset(const ae::render::LevelAsset& asset);

    // --- Queries ---

    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] const std::string& path() const { return path_; }

    [[nodiscard]] float sky_color_r() const;
    [[nodiscard]] float sky_color_g() const;
    [[nodiscard]] float sky_color_b() const;
    [[nodiscard]] float ambient_r() const;
    [[nodiscard]] float ambient_g() const;
    [[nodiscard]] float ambient_b() const;
    [[nodiscard]] float gravity() const;

    [[nodiscard]] const std::vector<ae::render::LevelSpawnPoint>& spawn_points() const;
    [[nodiscard]] const std::vector<ae::render::LevelCollisionBox>& collision_boxes() const;
    [[nodiscard]] const std::vector<ae::render::LevelMeshInstance>& mesh_instances() const;

    /// Read-access to the underlying compiled asset (for renderer wiring).
    [[nodiscard]] const ae::render::LevelAsset& level_asset() const { return asset_; }

    /// List of default maps that ship with the engine.
    static constexpr std::string_view kJavelin4 = "assets/compiled/levels/javelin4.aelevel";

private:
    ae::render::LevelAsset asset_;
    std::string path_;
};

}  // namespace ahamkara::game

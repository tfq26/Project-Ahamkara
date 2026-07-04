#include "ahamkara/game/map.h"

#include "ae/core/log.h"

#define AE_LOG_CATEGORY "Game"

namespace ahamkara::game {

bool Map::load(const std::string& path) {
    path_ = path;
    ae::render::CompiledLevelLoader loader;
    if (!loader.load(path, asset_)) {
        ae::log_warning_cat(AE_LOG_CATEGORY, "Failed to load map from " + path + ": " + loader.last_error());
        return false;
    }
    ae::log_info_cat(AE_LOG_CATEGORY, "Loaded map \"" + asset_.name + "\" from " + path +
                      " (" + std::to_string(asset_.collision_boxes.size()) + " colliders, " +
                      std::to_string(asset_.spawn_points.size()) + " spawns, " +
                      std::to_string(asset_.mesh_instances.size()) + " mesh instances)");
    return true;
}

Map Map::from_asset(const ae::render::LevelAsset& asset) {
    Map m;
    m.asset_ = asset;
    return m;
}

const std::string& Map::name() const { return asset_.name; }

float Map::sky_color_r() const { return asset_.sky_color_r; }
float Map::sky_color_g() const { return asset_.sky_color_g; }
float Map::sky_color_b() const { return asset_.sky_color_b; }
float Map::ambient_r() const { return asset_.ambient_r; }
float Map::ambient_g() const { return asset_.ambient_g; }
float Map::ambient_b() const { return asset_.ambient_b; }
float Map::gravity() const { return asset_.gravity; }

const std::vector<ae::render::LevelSpawnPoint>& Map::spawn_points() const { return asset_.spawn_points; }
const std::vector<ae::render::LevelCollisionBox>& Map::collision_boxes() const { return asset_.collision_boxes; }
const std::vector<ae::render::LevelMeshInstance>& Map::mesh_instances() const { return asset_.mesh_instances; }

}  // namespace ahamkara::game

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ae::render {

struct LevelSpawnPoint {
    float pos_x {0.0F};
    float pos_y {0.0F};
    float pos_z {0.0F};
    float yaw {0.0F};
    std::uint32_t team {0};
};

struct LevelCollisionBox {
    float min_x {0.0F};
    float min_z {0.0F};
    float max_x {0.0F};
    float max_z {0.0F};
    float top_y {0.0F};
    float bottom_y {0.0F};
    bool wall {false};
    bool jump_through {false};
    bool auto_step {true};
    std::uint32_t surface_material {0};
};

struct LevelMeshInstance {
    std::string mesh_asset_id {};
    std::string material_asset_id {};
    float pos_x {0.0F};
    float pos_y {0.0F};
    float pos_z {0.0F};
    float yaw {0.0F};
    float pitch {0.0F};
    float roll {0.0F};
    float scale_x {1.0F};
    float scale_y {1.0F};
    float scale_z {1.0F};
};

struct LevelAsset {
    std::string name {};
    float sky_color_r {0.3F};
    float sky_color_g {0.4F};
    float sky_color_b {0.6F};
    float ambient_r {0.05F};
    float ambient_g {0.05F};
    float ambient_b {0.1F};
    float gravity {20.0F};
    std::string skybox_material {};
    std::string ground_material {};
    std::vector<LevelSpawnPoint> spawn_points;
    std::vector<LevelCollisionBox> collision_boxes;
    std::vector<LevelMeshInstance> mesh_instances;
};

struct CompiledLevelFormat {
    static constexpr std::uint32_t magic = 0x5654454C;  // "LEVEL"
    static constexpr std::uint32_t version = 1;
};

[[nodiscard]] bool save_compiled_level(const std::string& path, const LevelAsset& level, std::string& error);

class CompiledLevelLoader {
public:
    [[nodiscard]] bool load(const std::string& path, LevelAsset& level);

    [[nodiscard]] const std::string& last_error() const { return error_; }

private:
    std::string error_;
};

} // namespace ae::render

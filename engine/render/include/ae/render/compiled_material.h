#pragma once

#include <cstdint>
#include <string>

namespace ae::render {

struct MaterialAsset {
    float base_color_r {1.0F};
    float base_color_g {1.0F};
    float base_color_b {1.0F};
    float base_color_a {1.0F};
    float metallic {0.0F};
    float roughness {1.0F};
    float emissive_r {0.0F};
    float emissive_g {0.0F};
    float emissive_b {0.0F};
    bool double_sided {false};
    std::string albedo_texture {};
    std::string normal_texture {};
    std::string orm_texture {};
    std::string emissive_texture {};
};

struct CompiledMaterialFile {
    static constexpr std::uint32_t magic = 0x54414d41; // AMAT
    static constexpr std::uint32_t version = 1;
};

[[nodiscard]] bool save_compiled_material(const std::string& path, const MaterialAsset& material, std::string& error);

class CompiledMaterialLoader {
public:
    [[nodiscard]] bool load(const std::string& path, MaterialAsset& material);

    [[nodiscard]] const std::string& last_error() const { return error_; }

private:
    std::string error_;
};

} // namespace ae::render

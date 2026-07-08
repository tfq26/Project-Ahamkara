#pragma once

#include "ae/render/gltf_loader.h"

#include <string>

namespace ae::render {

struct CompiledMeshFormat {
    static constexpr std::uint32_t magic = 0x4853454D;
    // v2 appends per-mesh UVs at the end of each mesh record (v1 is a prefix).
    static constexpr std::uint32_t version = 2;
};

[[nodiscard]] bool save_compiled_mesh(const std::string& path, const GltfModel& model, std::string& error);

class CompiledMeshLoader {
public:
    [[nodiscard]] bool load(const std::string& path, GltfModel& model);

    [[nodiscard]] const std::string& last_error() const { return error_; }

private:
    std::string error_;
};

} // namespace ae::render

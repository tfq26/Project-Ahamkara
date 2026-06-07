#pragma once

#include "ae/render/gltf_loader.h"

#include <string>

namespace ae::render {

struct CompiledMeshFormat {
    static constexpr std::uint32_t magic = 0x4853454D;
    static constexpr std::uint32_t version = 1;
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

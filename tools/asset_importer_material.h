#pragma once

#include "asset_importer_common.h"
#include "ae/render/compiled_material.h"

#include <string>

namespace asset_importer {

bool load_material_source(const std::filesystem::path& path, ae::render::MaterialAsset& material, std::string& error);
bool compile_material(const ImportEntry& entry);

} // namespace asset_importer

#pragma once

#include "asset_importer_common.h"
#include "ae/render/compiled_texture.h"

#include <string>

namespace asset_importer {

bool load_tga_texture(const std::filesystem::path& path, ae::render::TextureAsset& texture, std::string& error);
bool compile_texture(const ImportEntry& entry);

} // namespace asset_importer

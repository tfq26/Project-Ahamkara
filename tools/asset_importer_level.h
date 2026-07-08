#pragma once

#include "asset_importer_common.h"
#include "ae/render/compiled_level.h"

#include <string>

namespace asset_importer {

bool load_level_source(const std::filesystem::path& path, ae::render::LevelAsset& level, std::string& error);
bool compile_level(const ImportEntry& entry);

} // namespace asset_importer

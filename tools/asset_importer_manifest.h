#pragma once

#include "asset_importer_common.h"

#include <filesystem>
#include <vector>

namespace asset_importer {

bool read_manifest(const std::filesystem::path& manifest_path, std::vector<ImportEntry>& entries);

} // namespace asset_importer

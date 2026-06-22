#pragma once

#include <filesystem>
#include <string>

namespace asset_importer {

bool pack_assets(const std::filesystem::path& registry_path, const std::filesystem::path& output_pkg_path);

} // namespace asset_importer

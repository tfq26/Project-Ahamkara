#pragma once

#include "asset_importer_common.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace asset_importer {

std::uint64_t fnv1a_append(std::uint64_t hash, const void* data, std::size_t size);
std::string format_hash(std::uint64_t hash);
bool compute_file_hash(const std::filesystem::path& path, std::string& hash_string);
std::string make_asset_id(const std::filesystem::path& output_path);
std::filesystem::path registry_path_for_manifest(const std::filesystem::path& manifest_path);
bool write_registry(const std::filesystem::path& registry_path, const std::vector<ImportedAssetRecord>& records);
bool split_registry_row(const std::string& line, std::vector<std::string>& fields);
bool read_registry(const std::filesystem::path& registry_path, AssetRecordMap& records);
bool populate_record_identity(const ImportEntry& entry, ImportedAssetRecord& record);
bool finalize_record_output(const ImportEntry& entry, ImportedAssetRecord& record);
bool can_skip_import(const ImportEntry& entry, const ImportedAssetRecord& current_record, const AssetRecordMap& previous_records);

} // namespace asset_importer

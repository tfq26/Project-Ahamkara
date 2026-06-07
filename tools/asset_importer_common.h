#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace asset_importer {

struct ImportEntry {
    std::string kind;
    std::filesystem::path source;
    std::filesystem::path output;
    std::filesystem::path metadata;
};

struct ImportStats {
    int imported {0};
    int skipped {0};
    int failed {0};
};

struct ImportedAssetRecord {
    std::string asset_id;
    std::string kind;
    std::filesystem::path source;
    std::filesystem::path output;
    std::filesystem::path metadata;
    std::string source_hash;
    std::string metadata_hash;
    std::uintmax_t output_size {0};
};

using AssetRecordMap = std::unordered_map<std::string, ImportedAssetRecord>;

std::string trim(const std::string& value);
bool parse_bool_token(const std::string& value, bool& result);
bool parse_float_token(const std::string& value, float& result);
std::vector<std::string> split_tokens(const std::string& line);
std::filesystem::path resolve_path(const std::filesystem::path& manifest_dir, const std::string& token);

} // namespace asset_importer

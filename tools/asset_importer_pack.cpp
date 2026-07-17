#include "asset_importer_pack.h"
#include "asset_importer_registry.h"
#include "ae/core/log.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

#define AE_LOG_CATEGORY "Tools"

namespace asset_importer {

struct PkgHeader {
    char magic[4] {'A', 'P', 'K', 'G'};
    std::uint32_t version {1};
    std::uint32_t entry_count {0};
};

struct PkgEntry {
    std::string asset_id;
    std::filesystem::path filepath;
    std::uint64_t size {0};
    std::uint64_t offset {0};
};

bool pack_assets(const std::filesystem::path& registry_path, const std::filesystem::path& output_pkg_path) {
    AssetRecordMap records;
    if (!read_registry(registry_path, records)) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to read registry for packing: " + registry_path.string());
        return false;
    }

    std::vector<PkgEntry> pkg_entries;
    std::filesystem::path registry_dir = registry_path.parent_path();
    std::filesystem::path manifest_dir = registry_dir.parent_path();

    for (const auto& [id, record] : records) {
        if (record.kind == "model" || record.kind == "material" || record.kind == "texture") {
            std::filesystem::path final_path = record.output;
                if (!std::filesystem::exists(final_path)) {
                    final_path = registry_dir / record.output;
                    if (!std::filesystem::exists(final_path)) {
                        final_path = manifest_dir / record.output;
                        if (!std::filesystem::exists(final_path)) {
                            ae::log_error_cat(AE_LOG_CATEGORY, "Packed file not found: " + record.output.string());
                            return false;
                        }
                    }
                }

            PkgEntry entry;
            entry.asset_id = record.asset_id;
            entry.filepath = final_path;
            entry.size = std::filesystem::file_size(final_path);
            pkg_entries.push_back(entry);
        }
    }

    std::ofstream file(output_pkg_path, std::ios::binary);
    if (!file) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to create output package: " + output_pkg_path.string());
        return false;
    }

    PkgHeader header;
    header.entry_count = static_cast<std::uint32_t>(pkg_entries.size());

    // Write header placeholder
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Calculate metadata/entry table size to find where the first file data will begin
    // For each entry, we write:
    // - Length of asset_id (4 bytes)
    // - asset_id string
    // - offset (8 bytes)
    // - size (8 bytes)
    std::uint64_t current_offset = sizeof(PkgHeader);
    for (const auto& entry : pkg_entries) {
        current_offset += 4 + entry.asset_id.size() + 8 + 8;
    }

    // Now, we want to align the first file data to 4KB (4096 bytes)
    std::uint64_t first_data_offset = (current_offset + 4095) & ~4095ULL;
    std::uint64_t padding_before_first_data = first_data_offset - current_offset;

    // Fill in the offsets for each entry in our table
    std::uint64_t next_data_offset = first_data_offset;
    for (auto& entry : pkg_entries) {
        entry.offset = next_data_offset;
        // Align each subsequent file's start offset to 4KB
        std::uint64_t aligned_size = (entry.size + 4095) & ~4095ULL;
        next_data_offset += aligned_size;
    }

    // Write the entry table to the package file
    for (const auto& entry : pkg_entries) {
        std::uint32_t id_len = static_cast<std::uint32_t>(entry.asset_id.size());
        file.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        file.write(entry.asset_id.data(), id_len);
        file.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));
        file.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
    }

    // Write padding to align to 4096 bytes boundary before first file data
    if (padding_before_first_data > 0) {
        std::vector<char> padding(padding_before_first_data, 0);
        file.write(padding.data(), static_cast<std::streamsize>(padding_before_first_data));
    }

    // Write each file's data, with padding to 4096 bytes boundary
    for (const auto& entry : pkg_entries) {
        std::ifstream src(entry.filepath, std::ios::binary);
        if (!src) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Failed to read compiled file for packing: " + entry.filepath.string());
            return false;
        }

        file << src.rdbuf();

        std::uint64_t padding_needed = ((entry.size + 4095) & ~4095ULL) - entry.size;
        if (padding_needed > 0) {
            std::vector<char> padding(padding_needed, 0);
            file.write(padding.data(), static_cast<std::streamsize>(padding_needed));
        }
    }

    ae::log_info_cat(AE_LOG_CATEGORY, "Successfully packed " + std::to_string(pkg_entries.size()) + " assets into " + output_pkg_path.string());
    return true;
}

} // namespace asset_importer

#include "asset_importer_common.h"
#include "asset_importer_dispatch.h"
#include "asset_importer_manifest.h"
#include "asset_importer_registry.h"
#include "asset_importer_pack.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

using namespace asset_importer;

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::vector<ImportEntry> entries;

    const std::string_view command(argv[1]);
    std::filesystem::path registry_path;
    AssetRecordMap previous_records;
    const bool should_write_registry = command == "--manifest";

    if (command == "--manifest") {
        if (argc != 3) {
            print_usage();
            return 1;
        }

        if (!read_manifest(argv[2], entries)) {
            return 1;
        }

        registry_path = registry_path_for_manifest(argv[2]);
        read_registry(registry_path, previous_records);
    } else if (command == "--model") {
        if (argc != 4) {
            print_usage();
            return 1;
        }

        ImportEntry entry;
        entry.kind = "model";
        entry.source = argv[2];
        entry.output = argv[3];
        entries.push_back(std::move(entry));
    } else if (command == "--pack") {
        if (argc != 4) {
            print_usage();
            return 1;
        }
        std::filesystem::path reg_path = argv[2];
        std::filesystem::path pkg_path = argv[3];
        if (!pack_assets(reg_path, pkg_path)) {
            return 1;
        }
        return 0;
    } else if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    } else {
        print_usage();
        return 1;
    }

    ImportStats stats;
    std::vector<ImportedAssetRecord> records;
    records.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!std::filesystem::exists(entry.source)) {
            std::cerr << "Skipping missing source asset: " << entry.source << '\n';
            ++stats.skipped;
            continue;
        }

        ImportedAssetRecord record;
        if (!populate_record_identity(entry, record)) {
            ++stats.failed;
            continue;
        }

        if (should_write_registry && can_skip_import(entry, record, previous_records)) {
            if (!finalize_record_output(entry, record)) {
                ++stats.failed;
                continue;
            }

            std::cout << "skip   " << entry.source << " -> " << entry.output << '\n';
            records.push_back(std::move(record));
            ++stats.skipped;
            continue;
        }

        if (import_entry(entry)) {
            if (!finalize_record_output(entry, record)) {
                ++stats.failed;
                continue;
            }

            records.push_back(std::move(record));
            ++stats.imported;
        } else {
            ++stats.failed;
        }
    }

    if (should_write_registry && stats.failed == 0) {
        if (!write_registry(registry_path, records)) {
            ++stats.failed;
        } else {
            std::cout << "registry " << registry_path << '\n';
            // Auto-pack all compiled assets into assets.pkg
            std::filesystem::path pkg_path = registry_path.parent_path() / "assets.pkg";
            if (!pack_assets(registry_path, pkg_path)) {
                std::cerr << "Warning: Auto-packing assets failed.\n";
            }
        }
    }

    std::cout << "Imported: " << stats.imported << ", skipped: " << stats.skipped << ", failed: " << stats.failed << '\n';
    return stats.failed == 0 ? 0 : 1;
}

#include "asset_importer_registry.h"
#include "ae/core/log.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#define AE_LOG_CATEGORY "Tools"

namespace asset_importer {

std::uint64_t fnv1a_append(std::uint64_t hash, const void* data, std::size_t size) {
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    if (hash == 0) {
        hash = kOffsetBasis;
    }

    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= kPrime;
    }

    return hash;
}

std::string format_hash(std::uint64_t hash) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

bool compute_file_hash(const std::filesystem::path& path, std::string& hash_string) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::uint64_t hash = 0;
    char buffer[4096];
    while (file.good()) {
        file.read(buffer, static_cast<std::streamsize>(sizeof(buffer)));
        const auto count = static_cast<std::size_t>(file.gcount());
        if (count > 0) {
            hash = fnv1a_append(hash, buffer, count);
        }
    }

    if (!file.eof()) {
        return false;
    }

    hash_string = format_hash(hash);
    return true;
}

std::string make_asset_id(const std::filesystem::path& output_path) {
    std::filesystem::path normalized;
    bool inside_compiled_root = false;

    for (const auto& part : output_path.lexically_normal()) {
        if (inside_compiled_root) {
            normalized /= part;
        } else if (part == "compiled") {
            inside_compiled_root = true;
        }
    }

    if (!inside_compiled_root) {
        normalized = output_path.filename();
    }

    normalized.replace_extension();
    return normalized.generic_string();
}

std::filesystem::path registry_path_for_manifest(const std::filesystem::path& manifest_path) {
    return manifest_path.parent_path() / "compiled" / "asset_registry.tsv";
}

bool write_registry(const std::filesystem::path& registry_path, const std::vector<ImportedAssetRecord>& records) {
    std::error_code error;
    if (!registry_path.parent_path().empty()) {
        std::filesystem::create_directories(registry_path.parent_path(), error);
        if (error) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Failed to create registry directory for " + registry_path.string() + ": " + error.message());
            return false;
        }
    }

    std::ofstream file(registry_path);
    if (!file) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to create asset registry: " + registry_path.string());
        return false;
    }

    file << "asset_id\tkind\tsource\toutput\tmetadata\tsource_hash\tmetadata_hash\toutput_size\n";
    for (const auto& record : records) {
        file << record.asset_id << '\t'
             << record.kind << '\t'
             << record.source.generic_string() << '\t'
             << record.output.generic_string() << '\t'
             << record.metadata.generic_string() << '\t'
             << record.source_hash << '\t'
             << record.metadata_hash << '\t'
             << record.output_size << '\n';
    }

    return static_cast<bool>(file);
}

bool split_registry_row(const std::string& line, std::vector<std::string>& fields) {
    fields.clear();

    std::size_t start = 0;
    while (start <= line.size()) {
        const auto end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }

        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }

    return true;
}

bool read_registry(const std::filesystem::path& registry_path, AssetRecordMap& records) {
    records.clear();

    std::ifstream file(registry_path);
    if (!file) {
        return false;
    }

    std::string header;
    if (!std::getline(file, header)) {
        return false;
    }

    if (header != "asset_id\tkind\tsource\toutput\tmetadata\tsource_hash\tmetadata_hash\toutput_size") {
        ae::log_error_cat(AE_LOG_CATEGORY, "Asset registry header mismatch: " + registry_path.string());
        return false;
    }

    std::string line;
    std::vector<std::string> fields;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        split_registry_row(line, fields);
        if (fields.size() != 8) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Malformed asset registry row in " + registry_path.string());
            return false;
        }

        ImportedAssetRecord record;
        record.asset_id = fields[0];
        record.kind = fields[1];
        record.source = fields[2];
        record.output = fields[3];
        record.metadata = fields[4];
        record.source_hash = fields[5];
        record.metadata_hash = fields[6];

        try {
            record.output_size = static_cast<std::uintmax_t>(std::stoull(fields[7]));
        } catch (const std::exception&) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Invalid output_size in asset registry: " + registry_path.string());
            return false;
        }

        records[record.asset_id] = std::move(record);
    }

    return true;
}

bool populate_record_identity(const ImportEntry& entry, ImportedAssetRecord& record) {
    record.asset_id = make_asset_id(entry.output);
    record.kind = entry.kind;
    record.source = entry.source.lexically_normal();
    record.output = entry.output.lexically_normal();
    record.metadata = entry.metadata.lexically_normal();
    record.output_size = 0;

    if (!compute_file_hash(entry.source, record.source_hash)) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to hash source asset: " + entry.source.string());
        return false;
    }

    if (!entry.metadata.empty()) {
        if (!compute_file_hash(entry.metadata, record.metadata_hash)) {
            ae::log_error_cat(AE_LOG_CATEGORY, "Failed to hash metadata asset: " + entry.metadata.string());
            return false;
        }
    }

    return true;
}

bool finalize_record_output(const ImportEntry& entry, ImportedAssetRecord& record) {
    std::error_code error;
    record.output_size = std::filesystem::file_size(entry.output, error);
    if (error) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to stat compiled asset: " + entry.output.string() + ": " + error.message());
        return false;
    }

    return true;
}

bool can_skip_import(const ImportEntry& entry, const ImportedAssetRecord& current_record, const AssetRecordMap& previous_records) {
    const auto it = previous_records.find(current_record.asset_id);
    if (it == previous_records.end()) {
        return false;
    }

    const auto& previous = it->second;
    if (previous.kind != current_record.kind ||
        previous.source.lexically_normal() != current_record.source.lexically_normal() ||
        previous.output.lexically_normal() != current_record.output.lexically_normal() ||
        previous.metadata.lexically_normal() != current_record.metadata.lexically_normal() ||
        previous.source_hash != current_record.source_hash ||
        previous.metadata_hash != current_record.metadata_hash) {
        return false;
    }

    if (!std::filesystem::exists(entry.output)) {
        return false;
    }

    std::error_code error;
    const auto output_size = std::filesystem::file_size(entry.output, error);
    if (error) {
        return false;
    }

    return output_size == previous.output_size;
}

} // namespace asset_importer

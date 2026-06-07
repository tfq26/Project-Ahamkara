#include "asset_importer_manifest.h"

#include <fstream>
#include <iostream>
#include <string>

namespace asset_importer {

bool read_manifest(const std::filesystem::path& manifest_path, std::vector<ImportEntry>& entries) {
    std::ifstream file(manifest_path);
    if (!file) {
        std::cerr << "Failed to open manifest: " << manifest_path << '\n';
        return false;
    }

    const auto manifest_dir = manifest_path.parent_path();
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;

        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto tokens = split_tokens(line);
        if (tokens.size() < 3 || tokens.size() > 4) {
            std::cerr << manifest_path << ':' << line_number
                      << ": expected '<kind> <source> <output> [metadata]'\n";
            return false;
        }

        ImportEntry entry;
        entry.kind = tokens[0];
        entry.source = resolve_path(manifest_dir, tokens[1]);
        entry.output = resolve_path(manifest_dir, tokens[2]);
        if (tokens.size() == 4) {
            entry.metadata = resolve_path(manifest_dir, tokens[3]);
        }

        entries.push_back(std::move(entry));
    }

    return true;
}

} // namespace asset_importer

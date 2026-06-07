#include "asset_importer_common.h"

#include <sstream>
#include <stdexcept>

namespace asset_importer {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parse_bool_token(const std::string& value, bool& result) {
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        result = true;
        return true;
    }

    if (value == "false" || value == "0" || value == "no" || value == "off") {
        result = false;
        return true;
    }

    return false;
}

bool parse_float_token(const std::string& value, float& result) {
    try {
        result = std::stof(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> split_tokens(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

std::filesystem::path resolve_path(const std::filesystem::path& manifest_dir, const std::string& token) {
    std::filesystem::path path(token);
    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (manifest_dir / path).lexically_normal();
}

} // namespace asset_importer

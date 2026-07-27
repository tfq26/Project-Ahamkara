#include "wish/core/version_compatibility.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace wish::core {

void VersionCompatibilityTable::add_entry(VersionCompatibilityEntry entry) {
    entries_.push_back(std::move(entry));
}

const std::vector<VersionCompatibilityEntry>& VersionCompatibilityTable::entries() const {
    return entries_;
}

bool VersionCompatibilityTable::has_entries() const {
    return !entries_.empty();
}

std::vector<int> VersionCompatibilityTable::parse_version(std::string_view version) {
    std::vector<int> components;
    std::string current;

    for (const char c : version) {
        if (c == '.') {
            if (!current.empty()) {
                components.push_back(std::atoi(current.c_str()));
                current.clear();
            }
        } else if (c >= '0' && c <= '9') {
            current += c;
        } else {
            break;
        }
    }

    if (!current.empty()) {
        components.push_back(std::atoi(current.c_str()));
    }

    return components;
}

int VersionCompatibilityTable::compare_versions(const std::vector<int>& a, const std::vector<int>& b) {
    const std::size_t max_len = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < max_len; ++i) {
        const int a_val = i < a.size() ? a[i] : 0;
        const int b_val = i < b.size() ? b[i] : 0;
        if (a_val < b_val) return -1;
        if (a_val > b_val) return 1;
    }
    return 0;
}

VersionCompatibilityReport VersionCompatibilityTable::check_compatibility(
    std::string_view server_version,
    std::string_view client_version) const {

    VersionCompatibilityReport report;
    report.server_version = std::string(server_version);
    report.client_version = std::string(client_version);
    report.wish_code = WishErrorCode::kProtocolVersionMismatch;

    for (const auto& entry : entries_) {
        if (!version_matches_pattern(server_version, entry.version_pattern)) {
            continue;
        }

        const auto client_parsed = parse_version(client_version);
        const auto min_parsed = parse_version(entry.min_compatible_version);

        if (client_parsed.empty() || min_parsed.empty()) {
            report.compatible = false;
            report.severity = VersionSkewSeverity::Unknown;
            report.explanation = "Could not parse version strings for compatibility check.";
            report.resolution_hint = "Ensure client and server are using the same version of the software.";
            return report;
        }

        const int cmp = compare_versions(client_parsed, min_parsed);

        if (cmp >= 0) {
            report.compatible = true;
            report.severity = VersionSkewSeverity::Compatible;
            report.explanation = entry.description;
            report.wish_code = WishErrorCode::kProtocolError;
            report.resolution_hint = "No action needed \u2014 versions are compatible.";
            return report;
        }

        if (client_parsed.size() >= 1 && min_parsed.size() >= 1 &&
            client_parsed[0] < min_parsed[0]) {
            report.severity = VersionSkewSeverity::MajorSkew;
            report.explanation = "Major version mismatch \u2014 client is too old for this server.";
            report.resolution_hint = "Update the client to a newer version that matches the server.";
        } else {
            report.severity = VersionSkewSeverity::MinorSkew;
            report.explanation = "Minor version difference \u2014 client may not be compatible with all features.";
            report.resolution_hint = "Update the client to match the server\u2019s protocol version.";
        }

        report.compatible = false;
        return report;
    }

    report.compatible = false;
    report.severity = VersionSkewSeverity::Unknown;
    report.explanation = "Server version '" + std::string(server_version) +
                         "' is not in the compatibility table.";
    report.resolution_hint = "Ensure the server is running a registered protocol version.";

    return report;
}

std::string format_version_report(const VersionCompatibilityReport& report) {
    std::ostringstream stream;
    stream << "Version compatibility check: "
           << "server=" << report.server_version
           << " client=" << report.client_version
           << " compatible=" << (report.compatible ? "yes" : "no");

    if (!report.compatible) {
        stream << " severity=";
        switch (report.severity) {
            case VersionSkewSeverity::MinorSkew:
                stream << "MINOR";
                break;
            case VersionSkewSeverity::MajorSkew:
                stream << "MAJOR";
                break;
            case VersionSkewSeverity::Unknown:
                stream << "UNKNOWN";
                break;
            default:
                stream << "NONE";
                break;
        }
    }

    stream << " (" << report.explanation << ")";
    stream << " [" << report.resolution_hint << "]";

    return stream.str();
}

bool version_matches_pattern(std::string_view version, std::string_view pattern) {
    if (pattern == "*") {
        return true;
    }

    std::size_t v_idx = 0;
    std::size_t p_idx = 0;

    while (p_idx < pattern.size()) {
        if (pattern[p_idx] == '*') {
            return true;
        }

        if (v_idx >= version.size()) {
            return false;
        }

        if (pattern[p_idx] != version[v_idx]) {
            return false;
        }

        ++p_idx;
        ++v_idx;
    }

    return v_idx == version.size();
}

} // namespace wish::core

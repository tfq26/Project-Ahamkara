#pragma once

#include "wish/core/error_codes.h"
#include "wish/types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

/// Maximum length for a version string.
inline constexpr std::size_t kMaxVersionLength = 32;

/// Describes the severity of a version mismatch.
enum class VersionSkewSeverity : wish::u8 {
    Compatible = 0,     // Versions are compatible
    MinorSkew = 1,      // Minor version difference (likely compatible)
    MajorSkew = 2,      // Major version difference (likely incompatible)
    Unknown = 3,        // Version format not recognized
};

/// A detailed version compatibility report for diagnostics.
struct VersionCompatibilityReport {
    /// The server's protocol version.
    std::string server_version;

    /// The client's reported protocol version.
    std::string client_version;

    /// Whether the versions are compatible.
    bool compatible {false};

    /// The severity of the skew.
    VersionSkewSeverity severity {VersionSkewSeverity::Unknown};

    /// Human-readable explanation of the incompatibility.
    std::string explanation;

    /// Actionable suggestion for resolving the issue.
    std::string resolution_hint;

    /// The matching Wish error code for this skew.
    WishErrorCode wish_code {WishErrorCode::kProtocolError};
};

/// A single entry in the version compatibility table.
struct VersionCompatibilityEntry {
    std::string version_pattern;
    std::string min_compatible_version;
    std::string description;
};

/// The version compatibility table.
/// Maps version strings to their compatibility ranges.
class VersionCompatibilityTable {
public:
    /// Add a compatibility entry.
    void add_entry(VersionCompatibilityEntry entry);

    /// Check if a client version is compatible with the server version.
    [[nodiscard]] VersionCompatibilityReport check_compatibility(
        std::string_view server_version,
        std::string_view client_version) const;

    /// Get the list of registered entries.
    [[nodiscard]] const std::vector<VersionCompatibilityEntry>& entries() const;

    /// Check if the table has any entries.
    [[nodiscard]] bool has_entries() const;

private:
    std::vector<VersionCompatibilityEntry> entries_;

    /// Parse a version string into its numeric components.
    /// Returns empty vector on failure.
    [[nodiscard]] static std::vector<int> parse_version(std::string_view version);

    /// Compare two parsed version vectors.
    /// Returns negative if a < b, 0 if equal, positive if a > b.
    [[nodiscard]] static int compare_versions(const std::vector<int>& a, const std::vector<int>& b);
};

/// Build a diagnostic string from a version report for logging.
[[nodiscard]] std::string format_version_report(const VersionCompatibilityReport& report);

/// Check if a version string matches a given pattern (supports wildcard '*').
[[nodiscard]] bool version_matches_pattern(std::string_view version, std::string_view pattern);

} // namespace wish::core

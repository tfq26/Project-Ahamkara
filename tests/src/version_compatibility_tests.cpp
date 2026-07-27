#include "wish/core/version_compatibility.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_version_pattern_matching() {
    assert(wish::core::version_matches_pattern("1.0.0", "1.0.0"));
    assert(wish::core::version_matches_pattern("1.0.0", "1.0.*"));
    assert(wish::core::version_matches_pattern("1.0.5", "1.0.*"));
    assert(wish::core::version_matches_pattern("2.0.0", "*"));
    assert(!wish::core::version_matches_pattern("1.0.0", "2.0.0"));
    assert(!wish::core::version_matches_pattern("1.1.0", "1.0.*"));

    std::cout << "test_version_pattern_matching passed.\n";
}

void test_compatibility_table_basic() {
    wish::core::VersionCompatibilityTable table;

    wish::core::VersionCompatibilityEntry entry;
    entry.version_pattern = "1.0.*";
    entry.min_compatible_version = "1.0.0";
    entry.description = "Version 1.0.x is stable and supported.";
    table.add_entry(entry);

    auto report = table.check_compatibility("1.0.0", "1.0.0");
    assert(report.compatible);
    assert(report.severity == wish::core::VersionSkewSeverity::Compatible);

    report = table.check_compatibility("1.0.0", "1.0.5");
    assert(report.compatible);

    std::cout << "test_compatibility_table_basic passed.\n";
}

void test_version_incompatibility_detected() {
    wish::core::VersionCompatibilityTable table;

    wish::core::VersionCompatibilityEntry entry;
    entry.version_pattern = "2.0.*";
    entry.min_compatible_version = "2.0.0";
    entry.description = "Version 2.0.x current protocol.";
    table.add_entry(entry);

    auto report = table.check_compatibility("2.0.0", "1.5.0");
    assert(!report.compatible);
    assert(report.severity == wish::core::VersionSkewSeverity::MajorSkew);
    assert(!report.explanation.empty());
    assert(!report.resolution_hint.empty());

    std::cout << "test_version_incompatibility_detected passed.\n";
}

void test_compatibility_table_no_match() {
    wish::core::VersionCompatibilityTable table;

    auto report = table.check_compatibility("3.0.0", "3.0.0");
    assert(!report.compatible);
    assert(report.severity == wish::core::VersionSkewSeverity::Unknown);

    std::cout << "test_compatibility_table_no_match passed.\n";
}

void test_format_version_report() {
    wish::core::VersionCompatibilityTable table;

    wish::core::VersionCompatibilityEntry entry;
    entry.version_pattern = "1.0.*";
    entry.min_compatible_version = "1.0.0";
    entry.description = "Stable release";
    table.add_entry(entry);

    auto report = table.check_compatibility("1.0.0", "0.9.0");
    auto formatted = wish::core::format_version_report(report);
    assert(!formatted.empty());
    assert(formatted.find("server=1.0.0") != std::string::npos);
    assert(formatted.find("client=0.9.0") != std::string::npos);
    assert(formatted.find("compatible=no") != std::string::npos);

    std::cout << "test_format_version_report passed.\n";
}

void test_multiple_entries() {
    wish::core::VersionCompatibilityTable table;

    wish::core::VersionCompatibilityEntry v1;
    v1.version_pattern = "1.0.*";
    v1.min_compatible_version = "1.0.0";
    v1.description = "v1.0.x";
    table.add_entry(v1);

    wish::core::VersionCompatibilityEntry v2;
    v2.version_pattern = "2.0.*";
    v2.min_compatible_version = "2.0.0";
    v2.description = "v2.0.x";
    table.add_entry(v2);

    auto report1 = table.check_compatibility("1.0.5", "1.0.1");
    assert(report1.compatible);

    auto report2 = table.check_compatibility("2.0.0", "2.0.0");
    assert(report2.compatible);

    auto report3 = table.check_compatibility("1.0.0", "2.0.0");
    assert(report3.compatible);

    std::cout << "test_multiple_entries passed.\n";
}

void test_has_entries() {
    wish::core::VersionCompatibilityTable table;
    assert(!table.has_entries());

    wish::core::VersionCompatibilityEntry entry;
    entry.version_pattern = "1.0.*";
    entry.min_compatible_version = "1.0.0";
    table.add_entry(entry);

    assert(table.has_entries());
    assert(table.entries().size() == 1);

    std::cout << "test_has_entries passed.\n";
}

} // namespace

int main() {
    test_version_pattern_matching();
    test_compatibility_table_basic();
    test_version_incompatibility_detected();
    test_compatibility_table_no_match();
    test_format_version_report();
    test_multiple_entries();
    test_has_entries();

    std::cout << "All version compatibility tests passed.\n";
    return 0;
}

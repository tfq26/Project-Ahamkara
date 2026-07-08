#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ae/core/diagnostics.h"
#include "ae/core/log.h"

#define TEST(name)                                    \
    static void test_##name();                        \
    struct Register_##name {                          \
        Register_##name() {                           \
            tests.push_back(&test_##name);            \
            names.push_back(#name);                   \
        }                                             \
    } reg_##name;                                     \
    static void test_##name()

#define RUN_TESTS()                                  \
    do {                                              \
        int passed = 0, failed = 0;                   \
        for (std::size_t i_ = 0; i_ < tests.size(); ++i_) { \
            std::cout << "  " << names[i_] << "... "; \
            try {                                     \
                tests[i_]();                          \
                std::cout << "PASS\n";                \
                ++passed;                             \
            } catch (const std::exception& e) {       \
                std::cout << "FAIL (" << e.what() << ")\n"; \
                ++failed;                             \
            } catch (...) {                           \
                std::cout << "FAIL (unknown)\n";      \
                ++failed;                             \
            }                                         \
        }                                             \
        std::cout << "\n" << passed << "/" << (passed + failed) << " tests passed.\n"; \
        return (failed == 0) ? 0 : 1;                 \
    } while(0)

namespace {
std::vector<void (*)()> tests;
std::vector<const char*> names;

void require(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}
}  // anonymous namespace

// ===================================================================
// Test: collect_system_info returns non-empty data
// ===================================================================
TEST(collect_system_info) {
    auto info = ae::collect_system_info();
    require(!info.os_name.empty(), "OS name should not be empty");
    require(!info.cpu_brand.empty(), "CPU brand should not be empty");
    require(info.cpu_core_count > 0, "should detect at least 1 core");
    require(info.total_ram_bytes > 0, "should detect RAM > 0");
    std::cout << "    OS: " << info.os_name << "\n";
    std::cout << "    CPU: " << info.cpu_brand << " (" << info.cpu_core_count << " cores)\n";
    std::cout << "    RAM: " << (info.total_ram_bytes / (1024 * 1024)) << " MB\n";
}

// ===================================================================
// Test: collect_log_tail with missing file
// ===================================================================
TEST(collect_log_tail_missing) {
    auto lines = ae::collect_log_tail("/nonexistent/log.txt", 100);
    require(!lines.empty(), "should return at least 1 line indicating missing file");
    require(lines[0].find("not found") != std::string::npos,
            "should indicate file not found");
}

// ===================================================================
// Test: collect_log_tail with created file
// ===================================================================
TEST(collect_log_tail_with_file) {
    // Create a temp log file with some lines
    auto tmp_file = std::filesystem::temp_directory_path() / "ahamkara_test_log.txt";

    {
        std::ofstream f(tmp_file);
        for (int i = 0; i < 100; ++i) {
            f << "log line " << i << "\n";
        }
    }

    auto lines = ae::collect_log_tail(tmp_file, 50);
    require(lines.size() == 50, "should return last 50 lines");
    require(lines[0].find("log line 50") != std::string::npos,
            "first line should be line 50");
    require(lines[49].find("log line 99") != std::string::npos,
            "last line should be line 99");

    std::filesystem::remove(tmp_file);
}

// ===================================================================
// Test: write_diagnostic_bundle creates bundle with expected files
// ===================================================================
TEST(write_diagnostic_bundle) {
    // Write some log content so log tail capture works
    ae::init_file_logging("logs");

    auto tmp_dir = std::filesystem::temp_directory_path() / "ahamkara_diag_test";
    std::filesystem::remove_all(tmp_dir);

    auto bundle_path = ae::write_diagnostic_bundle(tmp_dir);
    require(!bundle_path.empty(), "bundle path should not be empty");
    require(std::filesystem::exists(bundle_path), "bundle directory should exist");

    // Check expected files exist
    require(std::filesystem::exists(bundle_path / "system_info.txt"),
            "system_info.txt should exist");
    require(std::filesystem::exists(bundle_path / "config_dump.txt"),
            "config_dump.txt should exist");
    require(std::filesystem::exists(bundle_path / "log_tail.txt"),
            "log_tail.txt should exist");
    require(std::filesystem::exists(bundle_path / "crash_summary.txt"),
            "crash_summary.txt should exist");

    // Verify content
    std::ifstream sysinfo(bundle_path / "system_info.txt");
    std::string first_line;
    std::getline(sysinfo, first_line);
    require(first_line.find("System Information") != std::string::npos,
            "system_info should start with header");

    // Cleanup
    std::filesystem::remove_all(tmp_dir);
}

// ===================================================================
// Test: list_diagnostic_bundles
// ===================================================================
TEST(list_diagnostic_bundles) {
    auto tmp_dir = std::filesystem::temp_directory_path() / "ahamkara_diag_list_test";
    std::filesystem::remove_all(tmp_dir);

    // No bundles initially
    auto bundles = ae::list_diagnostic_bundles(tmp_dir);
    require(bundles.empty(), "no bundles initially");

    // Create a couple bundles
    auto b1 = ae::write_diagnostic_bundle(tmp_dir);
    require(!b1.empty(), "first bundle");
    auto b2 = ae::write_diagnostic_bundle(tmp_dir);
    require(!b2.empty(), "second bundle");

    bundles = ae::list_diagnostic_bundles(tmp_dir);
    require(bundles.size() == 2, "should find 2 bundles");

    std::filesystem::remove_all(tmp_dir);
}

// ===================================================================
int main() {
    RUN_TESTS();
}

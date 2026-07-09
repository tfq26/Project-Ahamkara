#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ae/core/crash_handler.h"

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
// Test: capture stack trace (basic sanity)
// ===================================================================
TEST(capture_stack_trace) {
    auto frames = ae::capture_stack_trace(1);
    require(!frames.empty(), "stack trace should have at least 1 frame");
    require(frames[0].address != 0, "first frame should have an address");
    require(!frames[0].symbol.empty() && frames[0].symbol != "??",
            "first frame should have a symbol");
    std::cout << "    Top frame: " << frames[0].symbol << "\n";
}

// ===================================================================
// Test: signal_name returns correct names
// ===================================================================
TEST(signal_name_table) {
    require(std::string(ae::signal_name(11)) == "SIGSEGV", "signal 11 = SIGSEGV");
    require(std::string(ae::signal_name(6)) == "SIGABRT", "signal 6 = SIGABRT");
    require(std::string(ae::signal_name(8)) == "SIGFPE", "signal 8 = SIGFPE");
    require(std::string(ae::signal_name(4)) == "SIGILL", "signal 4 = SIGILL");
    require(std::string(ae::signal_name(10)) == "SIGBUS", "signal 10 = SIGBUS");
    require(std::string(ae::signal_name(99)) == "UNKNOWN", "unknown signal");
}

// ===================================================================
// Test: write crash dump to a temp directory
// ===================================================================
TEST(write_crash_dump) {
    auto tmp_dir = std::filesystem::temp_directory_path() / "ahamkara_crash_test";
    std::filesystem::remove_all(tmp_dir);

    ae::CrashContext ctx;
    ctx.signal_num = 11;
    ctx.signal_name = "SIGSEGV";
    ctx.timestamp_sec = 1234567890.0;
    ctx.fault_addr = reinterpret_cast<void*>(0xDEADBEEF);

    // Add some fake stack frames
    ctx.frames.push_back({0x100001234, "main", "0x1234"});
    ctx.frames.push_back({0x100005678, "update()", "0x5678"});

    auto path = ae::write_crash_dump(ctx, tmp_dir);
    require(!path.empty(), "crash dump path should not be empty");
    require(std::filesystem::exists(path), "crash dump file should exist");

    // Read back and verify
    auto read_ctx = ae::read_crash_dump(path);
    require(read_ctx.signal_name == "SIGSEGV", "signal name roundtrip");
    require(read_ctx.signal_num == 11, "signal num roundtrip");
    require(read_ctx.frames.size() == 2, "should have 2 frames");
    require(read_ctx.frames[0].address == 0x100001234, "frame 0 address roundtrip");

    // Cleanup
    std::filesystem::remove_all(tmp_dir);
}

// ===================================================================
// Test: list crash dumps
// ===================================================================
TEST(list_crash_dumps) {
    auto tmp_dir = std::filesystem::temp_directory_path() / "ahamkara_crash_list_test";
    std::filesystem::remove_all(tmp_dir);
    std::filesystem::create_directories(tmp_dir);

    // No dumps yet
    auto dumps = ae::list_crash_dumps(tmp_dir);
    require(dumps.empty(), "no dumps initially");

    // Write two dumps
    ae::CrashContext ctx;
    ctx.signal_num = 6;
    ctx.signal_name = "SIGABRT";
    ctx.timestamp_sec = 1000.0;

    auto p1 = ae::write_crash_dump(ctx, tmp_dir);
    require(!p1.empty(), "first dump written");

    ctx.signal_name = "SIGSEGV";
    ctx.timestamp_sec = 2000.0;
    auto p2 = ae::write_crash_dump(ctx, tmp_dir);
    require(!p2.empty(), "second dump written");

    dumps = ae::list_crash_dumps(tmp_dir);
    require(dumps.size() == 2, "should find 2 dumps");

    // Most recent first (SIGSEGV has later timestamp)
    require(dumps[0].filename().string().find("SIGSEGV") != std::string::npos,
            "most recent dump should be first");

    std::filesystem::remove_all(tmp_dir);
}

// ===================================================================
// Test: crash handler install/uninstall
// ===================================================================
TEST(handler_install) {
    auto& handler = ae::CrashHandler::instance();

    // Should not be installed initially
    handler.uninstall();  // ensure clean state
    require(!handler.installed(), "not installed initially");

    handler.install();
    require(handler.installed(), "installed after install()");

    handler.uninstall();
    require(!handler.installed(), "not installed after uninstall()");
}

// ===================================================================
// Test: read_crash_dump handles missing file gracefully
// ===================================================================
TEST(read_missing_dump) {
    auto ctx = ae::read_crash_dump("/nonexistent/path.dmp");
    require(ctx.signal_num == 0, "empty context for missing file");
    require(ctx.frames.empty(), "no frames for missing file");
}

// ===================================================================
int main() {
    RUN_TESTS();
}

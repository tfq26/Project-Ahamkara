#include "ae/core/console.h"
#include "ae/core/config.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Simple test framework macros (matching console_tests.cpp style)
#define TEST(name)                                   \
    do {                                             \
        printf("  TEST: %s ... ", name);              \
        fflush(stdout);

#define END_TEST(result)                             \
        printf(result ? "PASSED\n" : "FAILED\n");    \
        if (!(result)) { failures++; }               \
    } while(0)

static int failures = 0;

// ============================================================
// Console command boundary tests
// ============================================================

static void test_unknown_command_returns_error() {
    ae::Console console;

    // An unrecognized command must produce an actionable error, not a silent stub.
    console.execute("nonexistent_command_xyz");

    bool found_error = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        const auto& line = console.log_line(i);
        if (line.text.find("nonexistent_command_xyz") != std::string::npos ||
            line.text.find("Unknown command") != std::string::npos) {
            found_error = true;
            break;
        }
    }

    TEST("unknown command emits diagnostic");
    END_TEST(found_error);
}

static void test_echo_command_is_actionable() {
    ae::Console console;
    console.register_builtins();

    console.execute("echo hello from test");

    bool found_output = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        const auto& line = console.log_line(i);
        if (line.text.find("hello from test") != std::string::npos) {
            found_output = true;
            break;
        }
    }

    TEST("echo command produces output");
    END_TEST(found_output);
}

static void test_stub_command_not_silent() {
    ae::Console console;

    // Register a stub command like the reload_shaders pattern.
    console.register_command("test_stub", "A stub for testing",
        [](const std::vector<std::string>&, ae::Console& self) {
            self.print_tagged("Console",
                "Error: test_stub is not yet implemented — no contract exists.");
        });

    console.execute("test_stub");

    bool found_stub_error = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        const auto& line = console.log_line(i);
        if (line.text.find("Error:") != std::string::npos ||
            line.text.find("not yet implemented") != std::string::npos) {
            found_stub_error = true;
            break;
        }
    }

    TEST("stub command returns actionable error, not silent success");
    END_TEST(found_stub_error);
}

// ============================================================
// Console input history tests
// ============================================================

static void test_history_ring_buffer() {
    ae::Console console;

    console.submit_input("cmd_one");
    console.submit_input("cmd_two");
    console.submit_input("cmd_three");

    TEST("history has 3 entries");
    END_TEST(console.history_count() == 3);

    // Most recently submitted should be accessible via history_line.
    // We offset from oldest (0) to newest (count-1).
    bool found_third = false;
    for (int i = 0; i < console.history_count(); ++i) {
        if (console.history_line(i) == "cmd_three") {
            found_third = true;
            break;
        }
    }

    TEST("history contains 'cmd_three'");
    END_TEST(found_third);
}

static void test_history_max_capacity() {
    ae::Console console;

    // Exceed kMaxHistory entries.
    for (int i = 0; i < 100; ++i) {
        console.submit_input("cmd_" + std::to_string(i));
    }

    TEST("history does not exceed kMaxHistory");
    END_TEST(console.history_count() <= ae::Console::kMaxHistory);

    // The most recent entries should be present.
    bool found_last = false;
    for (int i = 0; i < console.history_count(); ++i) {
        if (console.history_line(i) == "cmd_99") {
            found_last = true;
            break;
        }
    }

    TEST("most recent entry is preserved");
    END_TEST(found_last);
}

// ============================================================
// ConfigRegistry snapshot/rollback tests
// ============================================================

static void test_config_snapshot_preserves_state() {
    ae::ConfigRegistry& reg = ae::ConfigRegistry::instance();

    // Use a temp file for reload testing.
    const auto tmp_dir = std::filesystem::temp_directory_path();
    const auto tmp_file = tmp_dir / "ahamkara_test_config_snapshot.cfg";

    // Write a config with known values.
    {
        std::ofstream out(tmp_file);
        out << "# Test config\n";
        // Register some test cvars using the same pattern ConfigVar uses internally.
        // The ConfigVar<T> template auto-registers. For direct testing, we register
        // via register_var with lambda reload fns.
        std::string test_val = "initial";
        reg.register_var(
            "test.snapshot.val",
            [&test_val](std::string_view v) { test_val = std::string(v); },
            [&test_val]() { return test_val; });
        out << "test.snapshot.val=initial\n";
    }

    // Reload initial state.
    reg.reload_from_file(tmp_file.string());

    // Take a snapshot.
    reg.take_snapshot();

    // Now apply a bad file (only unknown keys - should trigger rollback).
    {
        std::ofstream out(tmp_file);
        out << "unknown_key_1=value1\n";
        out << "unknown_key_2=value2\n";
    }

    const int updated = reg.reload_from_file(tmp_file.string());

    TEST("reload with only unknown keys returns 0 (rollback)");
    END_TEST(updated == 0);

    // Verify the cvar still has its initial value (was not mutated).
    // We can check by reading the serialized value.
    const std::string current = reg.get_value("test.snapshot.val");

    TEST("config rolled back to pre-reload state");
    END_TEST(current == "initial");

    // Clean up.
    std::filesystem::remove(tmp_file);
}

static void test_config_file_not_found_preserves_state() {
    ae::ConfigRegistry& reg = ae::ConfigRegistry::instance();

    // Use a path that does not exist — should preserve and emit diagnostic.
    const auto tmp_dir = std::filesystem::temp_directory_path();
    const auto nonexistent = tmp_dir / "ahamkara_test_nonexistent_XXXXX.cfg";

    // Register a known cvar.
    std::string test_val = "preserved_value";
    reg.register_var(
        "test.preserve.val",
        [&test_val](std::string_view v) { test_val = std::string(v); },
        [&test_val]() { return test_val; });

    // Ensure it's set to our expected value.
    reg.set_value("test.preserve.val", "preserved_value");

    const int result = reg.reload_from_file(nonexistent.string());

    TEST("nonexistent config file returns 0");
    END_TEST(result == 0);

    const std::string current = reg.get_value("test.preserve.val");

    TEST("config preserved after nonexistent file reload");
    END_TEST(current == "preserved_value");

    std::filesystem::remove(nonexistent);
}

// ============================================================
// Main
// ============================================================

int main() {
    printf("── Developer Tooling Boundary Tests ──\n\n");

    printf("--- Console command routing ---\n");
    test_unknown_command_returns_error();
    test_echo_command_is_actionable();
    test_stub_command_not_silent();

    printf("\n--- Console input history ---\n");
    test_history_ring_buffer();
    test_history_max_capacity();

    printf("\n--- Config snapshot/rollback ---\n");
    test_config_snapshot_preserves_state();
    test_config_file_not_found_preserves_state();

    printf("\n── %s ──\n",
           failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}

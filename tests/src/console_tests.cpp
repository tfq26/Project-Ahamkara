#include "ae/core/console.h"
#include "ae/core/config.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Simple test framework macros
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
// Console tests
// ============================================================

static void test_console_tokenizer() {
    // Tokenizer is private, so we test via execute and check output.
    ae::Console console;

    // Test echo
    console.execute("echo hello world");
    bool found_echo = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("hello world") != std::string::npos) {
            found_echo = true;
            break;
        }
    }
    TEST("echo command");
    END_TEST(found_echo);
}

static void test_console_help() {
    ae::Console console;
    console.register_builtins();

    console.execute("help");
    bool found_help_header = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("Available Console Commands") != std::string::npos) {
            found_help_header = true;
            break;
        }
    }
    TEST("help command");
    END_TEST(found_help_header);
}

static void test_console_clear() {
    ae::Console console;
    console.register_builtins();

    console.execute("echo something");
    int before = console.log_line_count();
    console.execute("clear");
    int after = console.log_line_count();

    // After clear, the count should be reset (just the "Console cleared." message).
    TEST("clear command resets log");
    END_TEST(after < before && after >= 1);
}

static void test_console_register_custom() {
    ae::Console console;

    bool custom_called = false;
    console.register_command("greet", "A test command",
        [&custom_called](const std::vector<std::string>& args, ae::Console& self) {
            custom_called = true;
            if (args.size() == 1) {
                self.print("Hello, " + args[0] + "!");
            } else {
                self.print("Hello, world!");
            }
        });

    console.execute("greet Agent");
    bool found_greet = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("Hello, Agent!") != std::string::npos) {
            found_greet = true;
            break;
        }
    }
    TEST("custom command with args");
    END_TEST(custom_called && found_greet);
}

static void test_console_unknown_command() {
    ae::Console console;
    console.register_builtins();

    console.execute("nonexistent_command_xyz");
    bool found_unknown = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("Unknown command") != std::string::npos) {
            found_unknown = true;
            break;
        }
    }
    TEST("unknown command shows error");
    END_TEST(found_unknown);
}

static void test_console_history() {
    ae::Console console;
    console.register_builtins();

    console.execute("echo first");
    console.execute("echo second");
    console.execute("echo third");

    TEST("history count after 3 commands");
    END_TEST(console.history_count() >= 3);

    TEST("history populated");
    END_TEST(console.history_count() > 0);
}

static void test_console_cvar_integration() {
    // Create a cvar via ConfigRegistry and verify console can read it
    ae::ConfigVar<float> test_cvar("test.player_speed", 5.0F);
    ae::Console console;
    console.register_builtins();
    console.set_config_registry(&ae::ConfigRegistry::instance());

    // Set a known value
    test_cvar.set(10.0F);

    // Read it back via cvar_get
    console.execute("cvar_get test.player_speed");
    bool found_value = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("test.player_speed") != std::string::npos) {
            found_value = true;
            break;
        }
    }
    TEST("cvar_get from console");
    END_TEST(found_value);
}

static void test_console_log_ring_buffer() {
    ae::Console console;
    console.register_builtins();

    // Fill past the ring buffer limit to verify wrapping
    for (int i = 0; i < ae::Console::kMaxLogLines + 50; ++i) {
        console.execute("echo line_" + std::to_string(i));
    }

    int count = console.log_line_count();
    TEST("ring buffer capped at max");
    END_TEST(count <= ae::Console::kMaxLogLines);
}

// ============================================================
// Input persistence and history tests
// ============================================================

static void test_console_unregister_command() {
    ae::Console console;
    console.register_builtins();

    // Register a test command
    bool called = false;
    console.register_command("test_cmd", "Test command",
        [&called](const std::vector<std::string>&, ae::Console& self) {
            called = true;
            self.print("test_cmd executed");
        });

    console.execute("test_cmd");
    TEST("registered command executes");
    END_TEST(called);

    // Unregister and verify it's gone
    called = false;
    console.unregister_command("test_cmd");
    console.execute("test_cmd");
    TEST("unregistered command no longer executes");
    END_TEST(!called);

    // Should see "Unknown command" for removed command
    bool found_unknown = false;
    for (int i = 0; i < console.log_line_count(); ++i) {
        if (console.log_line(i).text.find("Unknown command") != std::string::npos) {
            found_unknown = true;
            break;
        }
    }
    TEST("unregistered command displays error");
    END_TEST(found_unknown);
}

static void test_console_history_navigation() {
    ae::Console console;
    console.register_builtins();

    console.execute("echo first_command");
    console.execute("echo second_command");
    console.execute("echo third_command");

    TEST("history has 3 entries");
    END_TEST(console.history_count() >= 3);

    // History should contain the submitted lines (with "echo" prefix)
    bool found_second = false;
    for (int i = 0; i < console.history_count(); ++i) {
        if (console.history_line(i).find("second_command") != std::string::npos) {
            found_second = true;
            break;
        }
    }
    TEST("history contains second_command");
    END_TEST(found_second);
}

static void test_console_history_ring_wrap() {
    ae::Console console;
    console.register_builtins();

    // Fill past the history buffer limit to verify wrapping
    const int overflow = ae::Console::kMaxHistory + 10;
    for (int i = 0; i < overflow; ++i) {
        console.execute("echo cmd_" + std::to_string(i));
    }

    // History count should be capped at kMaxHistory
    TEST("history count capped at kMaxHistory");
    END_TEST(console.history_count() <= ae::Console::kMaxHistory);

    // Oldest entries should have been overwritten
    // cmd_0 should no longer be in history (wrapped out)
    bool found_oldest = false;
    for (int i = 0; i < console.history_count(); ++i) {
        if (console.history_line(i).find("cmd_0") != std::string::npos) {
            found_oldest = true;
            break;
        }
    }
    TEST("oldest history entry wrapped out");
    END_TEST(!found_oldest);
}

static void test_console_input_persistence() {
    // Test that the console preserves state between multiple execute calls.
    // This tests the conceptual equivalent of input persistence:
    // the console's state is not reset between calls.
    ae::Console console;
    console.register_builtins();

    // Execute a command
    console.execute("echo persistent_state");
    int count_after_first = console.log_line_count();
    TEST("log has entries after first execute");
    END_TEST(count_after_first > 0);

    // Execute another command - state should persist
    console.execute("echo still_persistent");
    int count_after_second = console.log_line_count();
    TEST("state persists across multiple execute calls");
    END_TEST(count_after_second > count_after_first);
}

// ============================================================
// ConfigRegistry reload error tests
// ============================================================

static void test_config_reload_invalid_file_preserves_old_state() {
    auto& registry = ae::ConfigRegistry::instance();

    // Register a test cvar
    std::string current_value = "original";
    registry.register_var("test.reload_var",
        [&current_value](std::string_view v) { current_value = std::string(v); },
        [&current_value]() { return current_value; });

    // Create a temp valid config file
    std::string tmp_path = std::filesystem::temp_directory_path().string() + "/ahamkara_test_config.cfg";
    {
        std::ofstream f(tmp_path);
        f << "test.reload_var=valid_value\n";
    }

    // Reload from valid file
    int updated = registry.reload_from_file(tmp_path);
    TEST("valid config file updates variables");
    END_TEST(updated >= 1);
    TEST("cvar value updated from valid file");
    END_TEST(current_value == "valid_value");

    // Now write an invalid config file (malformed, no '=' sign)
    {
        std::ofstream f(tmp_path);
        f << "this line has no equals sign\n";
        f << "another_bad_line\n";
        f << "test.reload_var=should_not_apply\n";
    }

    // Reload from invalid file — first two lines are malformed,
    // but the third is valid.  The old valid value for the malformed
    // lines doesn't apply (they aren't cvars). The valid line applies.
    updated = registry.reload_from_file(tmp_path);

    // The valid line should still apply since it has correct syntax
    TEST("valid line in mixed file still applies");
    END_TEST(current_value == "should_not_apply");

    // Rewrite with an entirely malformed file (no valid lines)
    {
        std::ofstream f(tmp_path);
        f << "completely invalid content without equals\n";
        f << "also bad\n";
    }

    std::string before_reload = current_value;
    updated = registry.reload_from_file(tmp_path);

    // Old state should be preserved since no valid key=value lines
    TEST("state preserved after fully invalid config reload");
    END_TEST(current_value == before_reload);
    TEST("reload returns 0 for invalid file");
    END_TEST(updated == 0);

    // Clean up
    std::filesystem::remove(tmp_path);
}

static void test_config_reload_nonexistent_file() {
    auto& registry = ae::ConfigRegistry::instance();

    // Register a test cvar
    std::string current_value = "persistent_value";
    registry.register_var("test.nonexistent_reload",
        [&current_value](std::string_view v) { current_value = std::string(v); },
        [&current_value]() { return current_value; });

    // Try to reload from a nonexistent file
    std::string bad_path = "/tmp/ahamkara_nonexistent_file_xyz.cfg";
    int result = registry.reload_from_file(bad_path);
    TEST("reload from nonexistent file returns 0");
    END_TEST(result == 0);
    TEST("cvar value preserved after nonexistent file reload");
    END_TEST(current_value == "persistent_value");
}

// ============================================================
// ConfigRegistry iteration tests
// ============================================================

static void test_config_registry_iteration() {
    // Manually register test cvars with the registry
    auto& registry = ae::ConfigRegistry::instance();
    registry.register_var("test.alpha", [](std::string_view v) {}, []() { return std::string("1.0"); });
    registry.register_var("test.beta",  [](std::string_view v) {}, []() { return std::string("42"); });
    registry.register_var("test.gamma", [](std::string_view v) {}, []() { return std::string("hello"); });

    auto keys = registry.all_keys();

    bool found_a = false, found_b = false, found_c = false;
    for (const auto& k : keys) {
        if (k == "test.alpha") found_a = true;
        if (k == "test.beta")  found_b = true;
        if (k == "test.gamma") found_c = true;
    }

    TEST("ConfigRegistry iteration finds registered cvars");
    END_TEST(found_a && found_b && found_c);

    TEST("ConfigRegistry count matches");
    END_TEST(registry.count() >= 3);
}

static void test_config_registry_get_set() {
    auto& registry = ae::ConfigRegistry::instance();
    registry.register_var("test.get_set_var",
        [](std::string_view v) { /* would set value */ },
        []() { return std::string("99.000000"); });

    std::string val = registry.get_value("test.get_set_var");
    TEST("ConfigRegistry get_value returns correct value");
    END_TEST(val.find("99") != std::string::npos);

    bool set_ok = registry.set_value("test.get_set_var", "42.5");
    TEST("ConfigRegistry set_value succeeds");
    END_TEST(set_ok);

    bool set_bad = registry.set_value("nonexistent.key", "foo");
    TEST("ConfigRegistry set_value on unknown key fails");
    END_TEST(!set_bad);
}

// ============================================================
// Main
// ============================================================

int main() {
    printf("Console Tests\n");
    printf("=============\n\n");

    test_console_tokenizer();
    test_console_help();
    test_console_clear();
    test_console_register_custom();
    test_console_unknown_command();
    test_console_history();
    test_console_cvar_integration();
    test_console_log_ring_buffer();
    test_console_unregister_command();
    test_console_history_navigation();
    test_console_history_ring_wrap();
    test_console_input_persistence();

    printf("\nConfigRegistry Tests\n");
    printf("====================\n\n");

    test_config_registry_iteration();
    test_config_registry_get_set();
    test_config_reload_invalid_file_preserves_old_state();
    test_config_reload_nonexistent_file();

    printf("\n");
    if (failures > 0) {
        printf("*** %d TEST(S) FAILED ***\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}

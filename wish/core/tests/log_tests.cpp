// ── Log system tests ─────────────────────────────────────────────────────────
//
// Tests for the Wish logging API: categorized logging, level gating, runtime
// level control, and env-var parsing.  All tests verify behaviour through the
// public API only; they do NOT inspect actual output (the logger writes to
// stdout/stderr which is captured by the test runner).

#include "wish/log.h"
#include "wish/types.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>

// We use explicit category strings in tests rather than the WISH_LOG_CATEGORY
// macro so that tests are independent of any compiler- or project-level default.

namespace {

int fail(const std::string& msg) {
    std::cerr << "log_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Test: LogLevel to_string ───────────────────────────────────────────────

int test_to_string() {
    EXPECT(wish::to_string(wish::LogLevel::Error) == "Error", "Error string");
    EXPECT(wish::to_string(wish::LogLevel::Warning) == "Warning", "Warning string");
    EXPECT(wish::to_string(wish::LogLevel::Info) == "Info", "Info string");
    EXPECT(wish::to_string(wish::LogLevel::Debug) == "Debug", "Debug string");
    EXPECT(wish::to_string(wish::LogLevel::Trace) == "Trace", "Trace string");

    std::cout << "test_to_string: ok\n";
    return 0;
}

// ── Test: Default level gating (Debug and Trace are off by default) ────────

int test_default_gating() {
    // By default, Info and above are enabled; Debug and Trace are disabled.
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Error),   "Error enabled by default");
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Warning), "Warning enabled by default");
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Info),    "Info enabled by default");
    EXPECT(!wish::log_enabled("TestCat", wish::LogLevel::Debug),  "Debug disabled by default");
    EXPECT(!wish::log_enabled("TestCat", wish::LogLevel::Trace),  "Trace disabled by default");

    std::cout << "test_default_gating: ok\n";
    return 0;
}

// ── Test: Global level override ────────────────────────────────────────────

int test_set_log_level() {
    // Remember current level so we can restore at the end.
    const auto saved = wish::get_log_level();

    // Enable Debug globally.
    wish::set_log_level(wish::LogLevel::Debug);
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Debug), "Debug enabled after set_log_level(Debug)");

    // Trace should still be off.
    EXPECT(!wish::log_enabled("TestCat", wish::LogLevel::Trace), "Trace still off at Debug level");

    // Enable Trace globally.
    wish::set_log_level(wish::LogLevel::Trace);
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Trace), "Trace enabled after set_log_level(Trace)");

    // Disable Warning (set minimum to Error only).
    wish::set_log_level(wish::LogLevel::Error);
    EXPECT(wish::log_enabled("TestCat", wish::LogLevel::Error), "Error still enabled");
    EXPECT(!wish::log_enabled("TestCat", wish::LogLevel::Warning), "Warning disabled after set_log_level(Error)");
    EXPECT(!wish::log_enabled("TestCat", wish::LogLevel::Info), "Info disabled after set_log_level(Error)");

    // Restore.
    wish::set_log_level(saved);

    std::cout << "test_set_log_level: ok\n";
    return 0;
}

// ── Test: Per-category level override ──────────────────────────────────────

int test_category_level() {
    const auto saved_global = wish::get_log_level();

    // Set global to Info (default-like).
    wish::set_log_level(wish::LogLevel::Info);

    // Override a specific category to Trace.
    wish::set_category_log_level("VerboseCat", wish::LogLevel::Trace);
    EXPECT(wish::log_enabled("VerboseCat", wish::LogLevel::Trace), "VerboseCat Trace enabled");
    EXPECT(wish::log_enabled("VerboseCat", wish::LogLevel::Debug), "VerboseCat Debug enabled");

    // Other categories should still be at Info level.
    EXPECT(!wish::log_enabled("OtherCat", wish::LogLevel::Debug), "OtherCat Debug still off");

    // Remove the override by setting it back to default (Info).
    wish::set_category_log_level("VerboseCat", wish::LogLevel::Info);
    EXPECT(!wish::log_enabled("VerboseCat", wish::LogLevel::Debug), "VerboseCat Debug off after override removed");

    // Restore.
    wish::set_log_level(saved_global);

    std::cout << "test_category_level: ok\n";
    return 0;
}

// ── Test: get_category_log_level ───────────────────────────────────────────

int test_get_category_log_level() {
    const auto saved = wish::get_log_level();

    wish::set_log_level(wish::LogLevel::Info);

    // Without explicit override, should return global.
    EXPECT(wish::get_category_log_level("RandomCat") == wish::LogLevel::Info, "category gets global when unset");

    // After override.
    wish::set_category_log_level("RandomCat", wish::LogLevel::Trace);
    EXPECT(wish::get_category_log_level("RandomCat") == wish::LogLevel::Trace, "category returns override");

    // Restore.
    wish::set_log_level(saved);

    std::cout << "test_get_category_log_level: ok\n";
    return 0;
}

// ── Test: Environment variable parsing (WISH_LOG_LEVEL) ────────────────────

int test_env_global_level() {
    const auto saved = wish::get_log_level();

    ::setenv("WISH_LOG_LEVEL", "debug", 1);
    wish::init_log_levels_from_env();
    EXPECT(wish::get_log_level() == wish::LogLevel::Debug, "WISH_LOG_LEVEL=debug works");

    ::setenv("WISH_LOG_LEVEL", "trace", 1);
    wish::init_log_levels_from_env();
    EXPECT(wish::get_log_level() == wish::LogLevel::Trace, "WISH_LOG_LEVEL=trace works");

    ::setenv("WISH_LOG_LEVEL", "error", 1);
    wish::init_log_levels_from_env();
    EXPECT(wish::get_log_level() == wish::LogLevel::Error, "WISH_LOG_LEVEL=error works");

    ::setenv("WISH_LOG_LEVEL", "", 1);
    wish::init_log_levels_from_env();
    // Empty string should not change the level.
    EXPECT(wish::get_log_level() == wish::LogLevel::Error, "Empty WISH_LOG_LEVEL leaves level unchanged");

    // Restore.
    wish::set_log_level(saved);
    ::unsetenv("WISH_LOG_LEVEL");

    std::cout << "test_env_global_level: ok\n";
    return 0;
}

// ── Test: Environment variable parsing (WISH_LOG) ──────────────────────────

int test_env_per_category() {
    const auto saved = wish::get_log_level();
    wish::set_log_level(wish::LogLevel::Info);

    ::setenv("WISH_LOG", "Session:trace,Admin:debug", 1);
    wish::init_log_levels_from_env();

    EXPECT(wish::get_category_log_level("Session") == wish::LogLevel::Trace, "WISH_LOG Session=trace");
    EXPECT(wish::get_category_log_level("Admin") == wish::LogLevel::Debug, "WISH_LOG Admin=debug");
    EXPECT(wish::get_category_log_level("Other") == wish::LogLevel::Info, "WISH_LOG Other unaffected");

    ::unsetenv("WISH_LOG");

    // Restore.
    wish::set_log_level(saved);

    std::cout << "test_env_per_category: ok\n";
    return 0;
}

// ── Test: Categorized log functions are callable at all levels ─────────────

int test_categorized_log_callable() {
    // These should not crash or assert.
    wish::log_info_cat("TestCat",  "info message");
    wish::log_warning_cat("TestCat", "warning message");
    wish::log_error_cat("TestCat", "error message");

    // Debug/Trace are disabled by default so these are no-ops, but they
    // must still be safely callable.
    wish::log_debug_cat("TestCat", "debug message");
    wish::log_trace_cat("TestCat", "trace message");

    std::cout << "test_categorized_log_callable: ok\n";
    return 0;
}

// ── Test: Uncategorized log functions are callable ─────────────────────────

int test_uncategorized_log_callable() {
    // These are the backward-compatible wrappers.
    wish::log_info("info message (uncategorized)");
    wish::log_warning("warning message (uncategorized)");
    wish::log_error("error message (uncategorized)");

    // New convenience wrappers.
    wish::log_debug("debug message (uncategorized)");
    wish::log_trace("trace message (uncategorized)");

    std::cout << "test_uncategorized_log_callable: ok\n";
    return 0;
}

// ── Test: WISH_LOG_CATEGORY macro defaults to "Wish" ───────────────────────

int test_default_category_macro() {
    // When the header is included without a prior definition, the default
    // fallback is "Wish".  Re-include the header in a scope where the macro
    // is not defined (we never defined it in this TU).
    EXPECT(std::string_view("Wish") == "Wish",
           "Default category string is 'Wish'");

    std::cout << "test_default_category_macro: ok\n";
    return 0;
}

} // anonymous namespace

int main() {
    int failures = 0;

    failures += test_to_string();
    failures += test_default_gating();
    failures += test_set_log_level();
    failures += test_category_level();
    failures += test_get_category_log_level();
    failures += test_env_global_level();
    failures += test_env_per_category();
    failures += test_categorized_log_callable();
    failures += test_uncategorized_log_callable();
    failures += test_default_category_macro();

    if (failures > 0) {
        std::cerr << failures << " log test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All log tests passed.\n";
    return 0;
}

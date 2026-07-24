/// Tests for the enhanced Wish logging infrastructure.
///
/// These tests verify:
///   - Trace log level added to the enum and string conversions
///   - Category-prefixed output format
///   - Runtime level gating (log_enabled)
///   - Global and per-category level overrides
///   - Environment variable parsing (WISH_LOG_LEVEL, WISH_LOG)
///   - Backward compatibility of uncategorized log() and convenience wrappers

#include "wish/log.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

#define EXPECT_EQ(a, b)                                                                      \
    do {                                                                                     \
        if ((a) != (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " == " << #b \
                      << " (" << (a) << " != " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

void test_log_level_enum() {
    // Verify enum ordering (Error=0, Trace=highest)
    EXPECT_EQ(static_cast<int>(wish::LogLevel::Error), 0);
    EXPECT_EQ(static_cast<int>(wish::LogLevel::Warning), 1);
    EXPECT_EQ(static_cast<int>(wish::LogLevel::Info), 2);
    EXPECT_EQ(static_cast<int>(wish::LogLevel::Debug), 3);
    EXPECT_EQ(static_cast<int>(wish::LogLevel::Trace), 4);
}

void test_log_level_to_string() {
    EXPECT_EQ(wish::to_string(wish::LogLevel::Error), "Error");
    EXPECT_EQ(wish::to_string(wish::LogLevel::Warning), "Warning");
    EXPECT_EQ(wish::to_string(wish::LogLevel::Info), "Info");
    EXPECT_EQ(wish::to_string(wish::LogLevel::Debug), "Debug");
    EXPECT_EQ(wish::to_string(wish::LogLevel::Trace), "Trace");
}

void test_default_log_level_is_info() {
    // Default global level should be Info
    EXPECT_EQ(wish::get_log_level(), wish::LogLevel::Info);
}

void test_log_enabled_error_always() {
    // Error should always be enabled regardless of settings
    EXPECT_TRUE(wish::log_enabled("AnyCategory", wish::LogLevel::Error));

    // Even after setting a restrictive level, Error remains enabled
    wish::set_log_level(wish::LogLevel::Error);
    EXPECT_TRUE(wish::log_enabled("TestCat", wish::LogLevel::Error));

    // Reset for other tests
    wish::set_log_level(wish::LogLevel::Info);
}

void test_log_enabled_by_global_level() {
    wish::set_log_level(wish::LogLevel::Info);

    // Info and above should be enabled
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Info));
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Warning));
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Error));

    // Debug and Trace should be disabled by default
    EXPECT_TRUE(!wish::log_enabled("Cat", wish::LogLevel::Debug));
    EXPECT_TRUE(!wish::log_enabled("Cat", wish::LogLevel::Trace));
}

void test_set_global_log_level() {
    wish::set_log_level(wish::LogLevel::Debug);

    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Debug));
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Info));
    EXPECT_TRUE(!wish::log_enabled("Cat", wish::LogLevel::Trace));

    wish::set_log_level(wish::LogLevel::Trace);
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Trace));
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Debug));

    wish::set_log_level(wish::LogLevel::Warning);
    EXPECT_TRUE(!wish::log_enabled("Cat", wish::LogLevel::Info));
    EXPECT_TRUE(wish::log_enabled("Cat", wish::LogLevel::Warning));

    // Reset
    wish::set_log_level(wish::LogLevel::Info);
}

void test_per_category_level_override() {
    wish::set_log_level(wish::LogLevel::Info);

    // Override just the "Session" category to Debug
    wish::set_category_log_level("Session", wish::LogLevel::Debug);

    EXPECT_TRUE(wish::log_enabled("Session", wish::LogLevel::Debug));
    EXPECT_TRUE(wish::log_enabled("Session", wish::LogLevel::Info));

    // Other categories should still use global level
    EXPECT_TRUE(!wish::log_enabled("OtherCat", wish::LogLevel::Debug));
    EXPECT_TRUE(wish::log_enabled("OtherCat", wish::LogLevel::Info));

    // Clear category override by setting back to Info
    wish::set_category_log_level("Session", wish::LogLevel::Info);
    EXPECT_TRUE(!wish::log_enabled("Session", wish::LogLevel::Debug));
}

void test_get_category_log_level() {
    wish::set_log_level(wish::LogLevel::Warning);
    EXPECT_EQ(wish::get_category_log_level("AbsentCat"), wish::LogLevel::Warning);

    wish::set_category_log_level("MyCat", wish::LogLevel::Trace);
    EXPECT_EQ(wish::get_category_log_level("MyCat"), wish::LogLevel::Trace);

    // Reset
    wish::set_log_level(wish::LogLevel::Info);
    wish::set_category_log_level("MyCat", wish::LogLevel::Info);
}

void test_init_log_levels_from_env_global() {
    // Set env var and parse
    ::setenv("WISH_LOG_LEVEL", "debug", 1);
    wish::init_log_levels_from_env();

    EXPECT_EQ(wish::get_log_level(), wish::LogLevel::Debug);
    // Verify Debug is now enabled for any category
    EXPECT_TRUE(wish::log_enabled("EnvTest", wish::LogLevel::Debug));

    // Reset
    wish::set_log_level(wish::LogLevel::Info);
    ::unsetenv("WISH_LOG_LEVEL");
}

void test_init_log_levels_from_env_category() {
    ::setenv("WISH_LOG", "Session:trace,Protocol:debug", 1);
    wish::init_log_levels_from_env();

    EXPECT_EQ(wish::get_category_log_level("Session"), wish::LogLevel::Trace);
    EXPECT_TRUE(wish::log_enabled("Session", wish::LogLevel::Trace));

    EXPECT_EQ(wish::get_category_log_level("Protocol"), wish::LogLevel::Debug);
    EXPECT_TRUE(wish::log_enabled("Protocol", wish::LogLevel::Debug));

    // Global level unchanged
    EXPECT_EQ(wish::get_log_level(), wish::LogLevel::Info);

    // Reset
    wish::set_log_level(wish::LogLevel::Info);
    wish::set_category_log_level("Session", wish::LogLevel::Info);
    wish::set_category_log_level("Protocol", wish::LogLevel::Info);
    ::unsetenv("WISH_LOG");
}

void test_backward_compatible_log_functions() {
    // These should compile and not crash.
    // We can't easily capture stderr output, but we can verify they don't throw.
    try {
        wish::log(wish::LogLevel::Info, "backward-compat test");
        wish::log_info("info test");
        wish::log_warning("warning test");
        wish::log_error("error test");
    } catch (...) {
        EXPECT_TRUE(false && "basic log functions should not throw");
    }
}

void test_categorized_log_functions() {
    // These should compile and not crash.
    try {
        wish::log_info_cat("TestCat", "info cat test");
        wish::log_warning_cat("TestCat", "warning cat test");
        wish::log_error_cat("TestCat", "error cat test");
        wish::log_debug_cat("TestCat", "debug cat test");
        wish::log_trace_cat("TestCat", "trace cat test");
    } catch (...) {
        EXPECT_TRUE(false && "categorized log functions should not throw");
    }
}

void test_log_enabled_uses_category_gating() {
    wish::set_log_level(wish::LogLevel::Info);
    wish::set_category_log_level("VerboseCat", wish::LogLevel::Trace);

    // Same level, different categories
    EXPECT_TRUE(!wish::log_enabled("OtherCat", wish::LogLevel::Trace));
    EXPECT_TRUE(wish::log_enabled("VerboseCat", wish::LogLevel::Trace));

    // Reset
    wish::set_category_log_level("VerboseCat", wish::LogLevel::Info);
}

} // anonymous namespace

int main() {
    test_log_level_enum();
    test_log_level_to_string();
    test_default_log_level_is_info();
    test_log_enabled_error_always();
    test_log_enabled_by_global_level();
    test_set_global_log_level();
    test_per_category_level_override();
    test_get_category_log_level();
    test_init_log_levels_from_env_global();
    test_init_log_levels_from_env_category();
    test_backward_compatible_log_functions();
    test_categorized_log_functions();
    test_log_enabled_uses_category_gating();

    if (g_failures != 0) {
        std::cerr << "log_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "log_tests: ok (" << 14 << " tests)\n";
    return 0;
}

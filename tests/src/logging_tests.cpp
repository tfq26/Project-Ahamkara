#include "ae/core/log.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_log_level_enum_order() {
    assert(static_cast<int>(ae::LogLevel::Error)   == 0);
    assert(static_cast<int>(ae::LogLevel::Warning)  == 1);
    assert(static_cast<int>(ae::LogLevel::Info)     == 2);
    assert(static_cast<int>(ae::LogLevel::Debug)    == 3);
    assert(static_cast<int>(ae::LogLevel::Trace)    == 4);
    std::cout << "test_log_level_enum_order passed.\n";
}

void test_to_string() {
    assert(ae::to_string(ae::LogLevel::Error)   == "Error");
    assert(ae::to_string(ae::LogLevel::Warning) == "Warning");
    assert(ae::to_string(ae::LogLevel::Info)    == "Info");
    assert(ae::to_string(ae::LogLevel::Debug)   == "Debug");
    assert(ae::to_string(ae::LogLevel::Trace)   == "Trace");
    std::cout << "test_to_string passed.\n";
}

void test_default_log_enabled() {
    // Default global level is Info: Error/Warning/Info are enabled, Debug/Trace are not
    assert(ae::log_enabled("Test", ae::LogLevel::Error));
    assert(ae::log_enabled("Test", ae::LogLevel::Warning));
    assert(ae::log_enabled("Test", ae::LogLevel::Info));
    assert(!ae::log_enabled("Test", ae::LogLevel::Debug));
    assert(!ae::log_enabled("Test", ae::LogLevel::Trace));
    std::cout << "test_default_log_enabled passed.\n";
}

void test_global_log_level_change() {
    ae::set_log_level(ae::LogLevel::Debug);
    assert(ae::log_enabled("Test", ae::LogLevel::Error));
    assert(ae::log_enabled("Test", ae::LogLevel::Warning));
    assert(ae::log_enabled("Test", ae::LogLevel::Info));
    assert(ae::log_enabled("Test", ae::LogLevel::Debug));
    assert(!ae::log_enabled("Test", ae::LogLevel::Trace));

    ae::set_log_level(ae::LogLevel::Trace);
    assert(ae::log_enabled("Test", ae::LogLevel::Trace));

    ae::set_log_level(ae::LogLevel::Warning);
    assert(ae::log_enabled("Test", ae::LogLevel::Error));
    assert(ae::log_enabled("Test", ae::LogLevel::Warning));
    assert(!ae::log_enabled("Test", ae::LogLevel::Info));
    assert(!ae::log_enabled("Test", ae::LogLevel::Debug));
    assert(!ae::log_enabled("Test", ae::LogLevel::Trace));

    // Reset
    ae::set_log_level(ae::LogLevel::Info);
    std::cout << "test_global_log_level_change passed.\n";
}

void test_per_category_override() {
    ae::set_log_level(ae::LogLevel::Info);

    // Default: no per-category override
    assert(!ae::log_enabled("Network", ae::LogLevel::Debug));

    // Override a specific category
    ae::set_category_log_level("Network", ae::LogLevel::Trace);
    assert(ae::log_enabled("Network", ae::LogLevel::Debug));
    assert(ae::log_enabled("Network", ae::LogLevel::Trace));

    // Other categories unaffected
    assert(!ae::log_enabled("Render", ae::LogLevel::Debug));

    // Override another category
    ae::set_category_log_level("Render", ae::LogLevel::Debug);
    assert(ae::log_enabled("Render", ae::LogLevel::Debug));
    assert(!ae::log_enabled("Render", ae::LogLevel::Trace));

    // Reset
    ae::set_category_log_level("Network", ae::LogLevel::Info);
    ae::set_category_log_level("Render", ae::LogLevel::Info);
    std::cout << "test_per_category_override passed.\n";
}

void test_get_log_level() {
    ae::set_log_level(ae::LogLevel::Warning);
    assert(ae::get_log_level() == ae::LogLevel::Warning);

    ae::set_log_level(ae::LogLevel::Info);
    assert(ae::get_log_level() == ae::LogLevel::Info);

    std::cout << "test_get_log_level passed.\n";
}

void test_get_category_log_level() {
    ae::set_log_level(ae::LogLevel::Info);

    // No override: returns global level
    assert(ae::get_category_log_level("Physics") == ae::LogLevel::Info);

    ae::set_category_log_level("Physics", ae::LogLevel::Debug);
    assert(ae::get_category_log_level("Physics") == ae::LogLevel::Debug);

    // Reset
    ae::set_category_log_level("Physics", ae::LogLevel::Info);
    std::cout << "test_get_category_log_level passed.\n";
}

void test_category_override_higher_than_global() {
    // Category override should take effect even if it's higher (more verbose) than global
    ae::set_log_level(ae::LogLevel::Warning);
    ae::set_category_log_level("Special", ae::LogLevel::Debug);

    // Global is Warning, so Debug should be disabled for normal categories
    assert(!ae::log_enabled("Normal", ae::LogLevel::Debug));

    // But "Special" has a per-category override to Debug
    assert(ae::log_enabled("Special", ae::LogLevel::Debug));

    // Reset
    ae::set_log_level(ae::LogLevel::Info);
    ae::set_category_log_level("Special", ae::LogLevel::Info);
    std::cout << "test_category_override_higher_than_global passed.\n";
}

void test_category_override_lower_than_global() {
    // Category override can also be more restrictive than global
    ae::set_log_level(ae::LogLevel::Debug);
    ae::set_category_log_level("Quiet", ae::LogLevel::Warning);

    // Global is Debug, so Info should be enabled for normal categories
    assert(ae::log_enabled("Normal", ae::LogLevel::Info));

    // But "Quiet" has a per-category override to Warning, so Info should be suppressed
    assert(!ae::log_enabled("Quiet", ae::LogLevel::Info));

    // Reset
    ae::set_log_level(ae::LogLevel::Info);
    ae::set_category_log_level("Quiet", ae::LogLevel::Info);
    std::cout << "test_category_override_lower_than_global passed.\n";
}

}  // namespace

int main() {
    test_log_level_enum_order();
    test_to_string();
    test_default_log_enabled();
    test_global_log_level_change();
    test_per_category_override();
    test_get_log_level();
    test_get_category_log_level();
    test_category_override_higher_than_global();
    test_category_override_lower_than_global();

    std::cout << "\nAll logging tests passed.\n";
    return 0;
}

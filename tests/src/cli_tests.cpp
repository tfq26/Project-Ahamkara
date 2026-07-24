#include "ae/core/cli_utils.h"

#include <cassert>
#include <iostream>

namespace {

// ===================================================================
// ae::parse_float_arg tests
// ===================================================================

void test_parse_float_arg_matches() {
    float result = ae::parse_float_arg("--speed=1.5", "speed", 0.0F);
    assert(result == 1.5F);
    std::cout << "test_parse_float_arg_matches passed.\n";
}

void test_parse_float_arg_default_on_mismatch() {
    float result = ae::parse_float_arg("--other=2.0", "speed", -1.0F);
    assert(result == -1.0F);
    std::cout << "test_parse_float_arg_default_on_mismatch passed.\n";
}

void test_parse_float_arg_default_on_invalid() {
    float result = ae::parse_float_arg("--speed=notanumber", "speed", 0.0F);
    assert(result == 0.0F);
    std::cout << "test_parse_float_arg_default_on_invalid passed.\n";
}

void test_parse_float_arg_negative() {
    float result = ae::parse_float_arg("--offset=-3.25", "offset", 0.0F);
    assert(result == -3.25F);
    std::cout << "test_parse_float_arg_negative passed.\n";
}

void test_parse_float_arg_zero() {
    float result = ae::parse_float_arg("--scale=0.0", "scale", 1.0F);
    assert(result == 0.0F);
    std::cout << "test_parse_float_arg_zero passed.\n";
}

void test_parse_float_arg_partial_prefix() {
    // "--speedup=" should not match key "speed" (exact match required)
    float result = ae::parse_float_arg("--speedup=5.0", "speed", -1.0F);
    assert(result == -1.0F);
    std::cout << "test_parse_float_arg_partial_prefix passed.\n";
}

void test_parse_float_arg_no_double_dash() {
    // Argument without "--" prefix should not match
    float result = ae::parse_float_arg("speed=1.0", "speed", -1.0F);
    assert(result == -1.0F);
    std::cout << "test_parse_float_arg_no_double_dash passed.\n";
}

void test_parse_float_arg_no_equals() {
    // Argument with "--key" but no "=value" should not match
    float result = ae::parse_float_arg("--speed", "speed", -1.0F);
    assert(result == -1.0F);
    std::cout << "test_parse_float_arg_no_equals passed.\n";
}

void test_parse_float_arg_empty_value() {
    float result = ae::parse_float_arg("--speed=", "speed", 42.0F);
    assert(result == 42.0F);  // empty string after "=" should fail to parse
    std::cout << "test_parse_float_arg_empty_value passed.\n";
}

// ===================================================================
// ae::parse_bool_arg tests
// ===================================================================

void test_parse_bool_arg_matches() {
    bool result = ae::parse_bool_arg("--verbose", "verbose");
    assert(result == true);
    std::cout << "test_parse_bool_arg_matches passed.\n";
}

void test_parse_bool_arg_no_match() {
    bool result = ae::parse_bool_arg("--quiet", "verbose");
    assert(result == false);
    std::cout << "test_parse_bool_arg_no_match passed.\n";
}

void test_parse_bool_arg_with_value() {
    // "--verbose=true" should NOT match --verbose (exact match required, no value)
    bool result = ae::parse_bool_arg("--verbose=true", "verbose");
    assert(result == false);
    std::cout << "test_parse_bool_arg_with_value passed.\n";
}

void test_parse_bool_arg_no_double_dash() {
    bool result = ae::parse_bool_arg("verbose", "verbose");
    assert(result == false);
    std::cout << "test_parse_bool_arg_no_double_dash passed.\n";
}

void test_parse_bool_arg_empty_string() {
    bool result = ae::parse_bool_arg("", "verbose");
    assert(result == false);
    std::cout << "test_parse_bool_arg_empty_string passed.\n";
}

void test_parse_bool_arg_partial_prefix() {
    // "--verb" should not match key "verbose"
    bool result = ae::parse_bool_arg("--verb", "verbose");
    assert(result == false);
    std::cout << "test_parse_bool_arg_partial_prefix passed.\n";
}

// ===================================================================
// ae::trim from CLI context tests
// ===================================================================

void test_trim_cli_arg() {
    // Simulate trimming a raw CLI argument string
    assert(ae::trim("  --speed=1.0  ") == "--speed=1.0");
    assert(ae::trim("\t--flag\n") == "--flag");
    std::cout << "test_trim_cli_arg passed.\n";
}

}  // namespace

int main() {
    std::cout << "--- CLI Argument Parsing Tests ---\n";

    // parse_float_arg
    test_parse_float_arg_matches();
    test_parse_float_arg_default_on_mismatch();
    test_parse_float_arg_default_on_invalid();
    test_parse_float_arg_negative();
    test_parse_float_arg_zero();
    test_parse_float_arg_partial_prefix();
    test_parse_float_arg_no_double_dash();
    test_parse_float_arg_no_equals();
    test_parse_float_arg_empty_value();

    // parse_bool_arg
    test_parse_bool_arg_matches();
    test_parse_bool_arg_no_match();
    test_parse_bool_arg_with_value();
    test_parse_bool_arg_no_double_dash();
    test_parse_bool_arg_empty_string();
    test_parse_bool_arg_partial_prefix();

    // trim in CLI context
    test_trim_cli_arg();

    std::cout << "\nAll CLI tests passed.\n";
    return 0;
}

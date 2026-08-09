// Unit tests for wish::admin::escape_html.
//
// The function is used by the HTTP admin server to render user-controlled
// strings safely. These tests exercise the five HTML metacharacters, common
// XSS payloads, and edge cases (empty input, unicode passthrough, consecutive
// special characters, and double-escaping of already-escaped input).
#include "wish/admin/html_escape.h"

#include <iostream>
#include <string>
#include <string_view>

namespace {

int expect_equal(const char* test_name, const std::string_view input, const std::string& expected) {
    const std::string actual = wish::admin::escape_html(input);
    if (actual == expected) {
        return 0;
    }
    std::cerr << test_name << " failed:\n"
              << "  input:    [" << input << "]\n"
              << "  expected: [" << expected << "]\n"
              << "  actual:   [" << actual << "]\n";
    return 1;
}

int test_escapes_each_special_character() {
    int failures = 0;
    failures += expect_equal("ampersand", "&", "&amp;");
    failures += expect_equal("less_than", "<", "&lt;");
    failures += expect_equal("greater_than", ">", "&gt;");
    failures += expect_equal("double_quote", "\"", "&quot;");
    failures += expect_equal("single_quote", "'", "&#39;");
    return failures;
}

int test_escapes_special_characters_in_context() {
    int failures = 0;
    failures += expect_equal("html_text",
                             "a < b & c > d",
                             "a &lt; b &amp; c &gt; d");
    failures += expect_equal("attribute_value",
                             "title=\"foo\"",
                             "title=&quot;foo&quot;");
    failures += expect_equal("js_string_quote",
                             "it's",
                             "it&#39;s");
    return failures;
}

int test_common_xss_payloads() {
    int failures = 0;
    failures += expect_equal("script_tag",
                             "<script>alert('xss')</script>",
                             "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;");
    failures += expect_equal("img_onerror",
                             "<img src=x onerror=alert(1)>",
                             "&lt;img src=x onerror=alert(1)&gt;");
    failures += expect_equal("svg_onload",
                             "<svg/onload=alert(1)>",
                             "&lt;svg/onload=alert(1)&gt;");
    failures += expect_equal("attribute_breakout",
                             "\"><script>alert(1)</script>",
                             "&quot;&gt;&lt;script&gt;alert(1)&lt;/script&gt;");
    failures += expect_equal("iframe_javascript_url",
                             "<iframe src=\"javascript:alert(1)\"></iframe>",
                             "&lt;iframe src=&quot;javascript:alert(1)&quot;&gt;&lt;/iframe&gt;");
    failures += expect_equal("body_onload",
                             "<body onload=alert('xss')>",
                             "&lt;body onload=alert(&#39;xss&#39;)&gt;");
    return failures;
}

int test_empty_and_plain_input() {
    int failures = 0;
    failures += expect_equal("empty", "", "");
    failures += expect_equal("plain_text", "hello world", "hello world");
    failures += expect_equal("numbers_and_punct", "3 < 5 and 7 > 2", "3 &lt; 5 and 7 &gt; 2");
    return failures;
}

int test_unicode_passthrough() {
    int failures = 0;
    // "café 日本語" encoded as UTF-8 bytes; non-ASCII bytes are preserved verbatim.
    failures += expect_equal("unicode",
                             "caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
                             "caf\xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    return failures;
}

int test_double_escaping_of_already_escaped_input() {
    int failures = 0;
    failures += expect_equal("already_escaped",
                             "&lt;script&gt;",
                             "&amp;lt;script&amp;gt;");
    return failures;
}

int test_consecutive_special_characters() {
    int failures = 0;
    failures += expect_equal("consecutive",
                             "<<>>\"'&&",
                             "&lt;&lt;&gt;&gt;&quot;&#39;&amp;&amp;");
    return failures;
}

} // namespace

int main() {
    int failures = 0;
    failures += test_escapes_each_special_character();
    failures += test_escapes_special_characters_in_context();
    failures += test_common_xss_payloads();
    failures += test_empty_and_plain_input();
    failures += test_unicode_passthrough();
    failures += test_double_escaping_of_already_escaped_input();
    failures += test_consecutive_special_characters();

    if (failures > 0) {
        std::cerr << failures << " html_escape test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All html_escape tests passed.\n";
    return 0;
}

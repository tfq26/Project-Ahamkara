// Unit tests for XSS-safety of the admin HTTP server root HTML page.
//
// These tests render the real GET / HTML page (HttpAdminServer::render_info_page)
// with attacker-controlled values in every dynamic string field (game name and
// player endpoints) and verify that the page is safe for HTML context:
//
//   * Raw XSS payloads never appear verbatim in the output.
//   * Each payload's HTML-escaped equivalent is present.
//   * No unescaped tag opener (<script, <img, <svg, ...) survives rendering.
//   * Benign values and numeric server fields pass through unchanged.
//
// This complements tests/unit/admin/html_escape_tests.cpp (which exercises the
// escape_html() primitive in isolation) by validating the escaping at the
// rendered-document level, directly covering the acceptance criterion that
// XSS is not possible in the HTML returned by GET /.
#include "wish/admin/admin_server.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

void fail(const std::string& msg) {
    std::cerr << "admin_info_page_xss_tests failed: " << msg << '\n';
    ++g_failures;
}

void expect_contains(const std::string& haystack, const std::string& needle, const std::string& label) {
    if (haystack.find(needle) == std::string::npos) {
        fail(label + ": expected to find \"" + needle + "\"");
    }
}

void expect_not_contains(const std::string& haystack, const std::string& needle, const std::string& label) {
    if (haystack.find(needle) != std::string::npos) {
        fail(label + ": unexpected raw \"" + needle + "\" found");
    }
}

wish::admin::ServerStatus make_status() {
    wish::admin::ServerStatus status {};
    status.game_name = "Ahamkara Engine";
    status.game_port = 7777;
    status.admin_port = 8888;
    status.tick_rate = 60.0F;
    status.server_tick = 42;
    status.max_players = 16;
    status.uptime_seconds = 3661.0F;
    status.match_active = true;
    status.match_elapsed_seconds = 180.0F;
    status.match_duration_seconds = 900.0F;
    status.match_remaining_seconds = 720.0F;
    status.disconnect_timeout_seconds = 10.0F;
    return status;
}

// Tag openers that, if they appeared unescaped in the page, would allow the
// browser to parse a new HTML element. The static page template does not
// contain any of these, and escape_html() rewrites the '<' in every one of
// them, so none may ever appear in rendered output.
const std::vector<std::string> kForbiddenRawTagOpeners = {
    "<script",
    "<img",
    "<svg",
    "<iframe",
    "<object",
    "<embed",
};

struct XssCase {
    const char* name;
    const char* payload;
    const char* escaped; // expected escape_html(payload) for a single pass
};

// Payloads that attempt to break out of HTML text/attribute context. The
// expected `escaped` strings are hard-coded independent expectations (not
// computed from escape_html at runtime) so the test cannot be tautological.
const std::vector<XssCase> kPayloads = {
    {"script_alert",
     "<script>alert('xss')</script>",
     "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;"},
    {"img_onerror",
     "<img src=x onerror=alert(1)>",
     "&lt;img src=x onerror=alert(1)&gt;"},
    {"svg_onload",
     "<svg/onload=alert(1)>",
     "&lt;svg/onload=alert(1)&gt;"},
    {"iframe_javascript_url",
     "<iframe src=\"javascript:alert(1)\"></iframe>",
     "&lt;iframe src=&quot;javascript:alert(1)&quot;&gt;&lt;/iframe&gt;"},
    {"double_quote_breakout",
     "\"><script>alert(1)</script>",
     "&quot;&gt;&lt;script&gt;alert(1)&lt;/script&gt;"},
    {"single_quote_breakout",
     "' onmouseover='alert(1)' x='",
     "&#39; onmouseover=&#39;alert(1)&#39; x=&#39;"},
    {"attribute_and_tag_breakout",
     "\"><img src=x onerror=alert(2)>",
     "&quot;&gt;&lt;img src=x onerror=alert(2)&gt;"},
    {"body_onload",
     "<body onload=alert('xss')>",
     "&lt;body onload=alert(&#39;xss&#39;)&gt;"},
    {"mixed_metacharacters",
     "a<b>c&d\"e'f",
     "a&lt;b&gt;c&amp;d&quot;e&#39;f"},
};

void assert_rendered_page_safe(const std::string& html, const std::string& label) {
    for (const auto& raw : kForbiddenRawTagOpeners) {
        std::string msg = label;
        msg += " forbidden raw [";
        msg += raw;
        msg += "]";
        expect_not_contains(html, raw, msg);
    }
}

void check_payload_in_game_name(const XssCase& c) {
    wish::admin::ServerStatus status = make_status();
    status.game_name = c.payload;

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    const std::string label = std::string("game_name[") + c.name + "]";
    expect_not_contains(html, c.payload, label + " raw payload must not appear");
    expect_contains(html, c.escaped, label + " escaped form must appear");
    assert_rendered_page_safe(html, label);
}

void check_payload_in_player_endpoint(const XssCase& c) {
    wish::admin::ServerStatus status = make_status();

    wish::admin::PlayerStatus p1;
    p1.endpoint = c.payload;
    p1.seconds_since_seen = 1.0F;
    status.players.push_back(p1);

    wish::admin::PlayerStatus p2;
    p2.endpoint = "10.0.0.5:7777";
    p2.seconds_since_seen = 30.0F;
    status.players.push_back(p2);

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    const std::string label = std::string("player_endpoint[") + c.name + "]";
    expect_not_contains(html, c.payload, label + " raw payload must not appear");
    expect_contains(html, c.escaped, label + " escaped form must appear");
    expect_contains(html, "10.0.0.5:7777", label + " benign endpoint must survive");
    assert_rendered_page_safe(html, label);
}

int test_xss_payloads_are_neutralized() {
    for (const auto& c : kPayloads) {
        check_payload_in_game_name(c);
        check_payload_in_player_endpoint(c);
    }
    return g_failures;
}

int test_all_payloads_together() {
    wish::admin::ServerStatus status = make_status();
    status.game_name = kPayloads[0].payload;

    for (std::size_t i = 1; i < kPayloads.size(); ++i) {
        wish::admin::PlayerStatus p;
        p.endpoint = kPayloads[i].payload;
        p.seconds_since_seen = static_cast<float>(i);
        status.players.push_back(p);
    }

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    for (const auto& c : kPayloads) {
        expect_not_contains(html, c.payload, "combined raw [" + std::string(c.name) + "] must not appear");
        expect_contains(html, c.escaped, "combined escaped [" + std::string(c.name) + "] must appear");
    }
    assert_rendered_page_safe(html, "combined");
    return g_failures;
}

int test_benign_content_passes_through() {
    wish::admin::ServerStatus status = make_status();
    status.game_name = "Ahamkara Engine <Dev> & Friends";

    wish::admin::PlayerStatus p1;
    p1.endpoint = "10.0.0.1:7777";
    p1.seconds_since_seen = 2.5F;
    status.players.push_back(p1);

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    expect_contains(html, "Ahamkara Engine &lt;Dev&gt; &amp; Friends", "game name escaped");
    expect_contains(html, "10.0.0.1:7777", "benign endpoint unchanged");
    expect_not_contains(html, "Ahamkara Engine <Dev> & Friends", "raw game name must not appear");
    return g_failures;
}

int test_numeric_fields_cannot_carry_markup() {
    wish::admin::ServerStatus status = make_status();
    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    expect_contains(html, "7777", "game port");
    expect_contains(html, "8888", "admin port");
    expect_contains(html, "60 Hz", "tick rate");
    expect_contains(html, "42", "server tick");
    expect_contains(html, "16", "max players");
    expect_contains(html, "1h 1m 1s", "uptime formatted");
    assert_rendered_page_safe(html, "numeric");
    return g_failures;
}

int test_page_structure() {
    wish::admin::ServerStatus status = make_status();
    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    expect_contains(html, "<!DOCTYPE html>", "doctype");
    expect_contains(html, "<title>Ahamkara", "title");
    expect_contains(html, "<body>", "body open tag");
    expect_contains(html, "</body>", "body close tag");
    expect_contains(html, "</html>", "html close tag");
    return g_failures;
}

} // namespace

int main() {
    test_xss_payloads_are_neutralized();
    test_all_payloads_together();
    test_benign_content_passes_through();
    test_numeric_fields_cannot_carry_markup();
    test_page_structure();

    if (g_failures > 0) {
        std::cerr << g_failures << " admin info page XSS test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All admin info page XSS tests passed.\n";
    return 0;
}

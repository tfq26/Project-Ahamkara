#include "wish/admin/metrics_collector.h"

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void test_empty_metrics() {
    const wish::admin::MetricsCollector collector {};
    const std::string output = collector.render_prometheus();

    // Should contain all HELP/TYPE lines with zero values
    assert(output.find("# HELP wish_active_sessions") != std::string::npos);
    assert(output.find("# TYPE wish_active_sessions gauge") != std::string::npos);
    assert(output.find("wish_active_sessions 0") != std::string::npos);

    assert(output.find("# HELP wish_active_activities") != std::string::npos);
    assert(output.find("wish_active_activities 0") != std::string::npos);

    assert(output.find("# HELP wish_registered_servers") != std::string::npos);
    assert(output.find("wish_registered_servers 0") != std::string::npos);

    assert(output.find("# HELP wish_total_errors_total") != std::string::npos);
    assert(output.find("# TYPE wish_total_errors_total counter") != std::string::npos);
    assert(output.find("wish_total_errors_total 0") != std::string::npos);

    assert(output.find("# HELP wish_heartbeats_received_total") != std::string::npos);
    assert(output.find("wish_heartbeats_received_total 0") != std::string::npos);

    assert(output.find("# HELP wish_uptime_seconds") != std::string::npos);
    assert(output.find("wish_uptime_seconds 0") != std::string::npos);

    std::cout << "test_empty_metrics passed.\n";
}

void test_recorded_metrics() {
    wish::admin::MetricsCollector collector {};

    wish::admin::Metrics m {};
    m.active_sessions = 3;
    m.active_activities = 5;
    m.registered_servers = 2;
    m.total_errors = 42;
    m.heartbeats_received = 99;
    m.uptime_seconds = 3600;

    collector.record(m);
    const std::string output = collector.render_prometheus();

    // Verify each key/value pair
    assert(output.find("wish_active_sessions 3") != std::string::npos);
    assert(output.find("wish_active_activities 5") != std::string::npos);
    assert(output.find("wish_registered_servers 2") != std::string::npos);
    assert(output.find("wish_total_errors_total 42") != std::string::npos);
    assert(output.find("wish_heartbeats_received_total 99") != std::string::npos);
    assert(output.find("wish_uptime_seconds 3600") != std::string::npos);

    std::cout << "test_recorded_metrics passed.\n";
}

void test_metrics_output_format() {
    wish::admin::MetricsCollector collector {};

    wish::admin::Metrics m {};
    m.active_sessions = 1;
    collector.record(m);
    const std::string output = collector.render_prometheus();

    // Verify the overall structure: HELP, TYPE, then value on separate lines
    // The lines should be in order: HELP, TYPE, value for each metric
    const std::string expected_help = "# HELP wish_active_sessions Current number of active sessions";
    const std::string expected_type = "# TYPE wish_active_sessions gauge";
    const std::string expected_value = "wish_active_sessions 1";

    const auto help_pos = output.find(expected_help);
    const auto type_pos = output.find(expected_type);
    const auto value_pos = output.find(expected_value);

    assert(help_pos != std::string::npos);
    assert(type_pos != std::string::npos);
    assert(value_pos != std::string::npos);

    // HELP should come before TYPE, which should come before the value
    assert(help_pos < type_pos);
    assert(type_pos < value_pos);

    std::cout << "test_metrics_output_format passed.\n";
}

void test_record_overwrites() {
    wish::admin::MetricsCollector collector {};

    wish::admin::Metrics m1 {};
    m1.active_sessions = 10;
    collector.record(m1);

    wish::admin::Metrics m2 {};
    m2.active_sessions = 20;
    collector.record(m2);

    const std::string output = collector.render_prometheus();
    assert(output.find("wish_active_sessions 20") != std::string::npos);
    assert(output.find("wish_active_sessions 10") == std::string::npos);

    std::cout << "test_record_overwrites passed.\n";
}

} // namespace

int main() {
    test_empty_metrics();
    test_recorded_metrics();
    test_metrics_output_format();
    test_record_overwrites();
    std::cout << "All metrics tests passed.\n";
    return 0;
}

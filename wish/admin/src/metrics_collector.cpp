#include "wish/admin/metrics_collector.h"

#include <sstream>

namespace wish::admin {

void MetricsCollector::record(const Metrics& snapshot) {
    current_ = snapshot;
}

std::string MetricsCollector::render_prometheus() const {
    std::ostringstream stream;

    auto emit_gauge = [&](const char* name, const char* help, wish::u64 value) {
        stream << "# HELP " << name << ' ' << help << '\n'
               << "# TYPE " << name << " gauge\n"
               << name << ' ' << value << '\n';
    };

    auto emit_counter = [&](const char* name, const char* help, wish::u64 value) {
        stream << "# HELP " << name << ' ' << help << '\n'
               << "# TYPE " << name << " counter\n"
               << name << ' ' << value << '\n';
    };

    emit_gauge("wish_active_sessions",
               "Current number of active sessions",
               current_.active_sessions);

    emit_gauge("wish_active_activities",
               "Current number of running activities",
               current_.active_activities);

    emit_gauge("wish_registered_servers",
               "Number of registered game servers",
               current_.registered_servers);

    emit_counter("wish_total_errors_total",
                 "Total errors processed",
                 current_.total_errors);

    emit_counter("wish_heartbeats_received_total",
                 "Heartbeat requests received",
                 current_.heartbeats_received);

    emit_gauge("wish_uptime_seconds",
               "Server uptime in seconds",
               current_.uptime_seconds);

    return stream.str();
}

} // namespace wish::admin

#pragma once

#include "wish/types.h"

#include <string>

namespace wish::admin {

struct Metrics {
    wish::u64 active_sessions = 0;
    wish::u64 active_activities = 0;
    wish::u64 registered_servers = 0;
    wish::u64 total_errors = 0;
    wish::u64 heartbeats_received = 0;
    wish::u64 uptime_seconds = 0;
};

class MetricsCollector {
  public:
    void record(const Metrics& snapshot);

    // Render in Prometheus text format (Content-Type: text/plain; version=0.0.4)
    // Example output:
    //   # HELP wish_active_sessions Current active sessions
    //   # TYPE wish_active_sessions gauge
    //   wish_active_sessions 3
    std::string render_prometheus() const;

  private:
    Metrics current_ {};
};

} // namespace wish::admin

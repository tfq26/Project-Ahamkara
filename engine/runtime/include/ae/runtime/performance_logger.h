#pragma once

#include "ae/runtime/metrics.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace ae {

// =============================================================================
// PerformanceLogger — writes CSV frame-metrics to disk for post-hoc analysis.
// =============================================================================

class PerformanceLogger {
public:
    PerformanceLogger() = default;

    PerformanceLogger(const PerformanceLogger&) = delete;
    PerformanceLogger& operator=(const PerformanceLogger&) = delete;
    PerformanceLogger(PerformanceLogger&&) = delete;
    PerformanceLogger& operator=(PerformanceLogger&&) = delete;

    ~PerformanceLogger() { close(); }

    [[nodiscard]] bool open(const std::string& prefix);
    void close();
    [[nodiscard]] bool is_open() const { return file_ != nullptr; }
    void log(uint64_t frame_number, const RuntimeMetricsSnapshot& metrics);

private:
    FILE* file_ {nullptr};
};

}  // namespace ae

#include "ae/core/telemetry.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#define AE_LOG_CATEGORY "Telemetry"

namespace ae {

// ===================================================================
// TelemetryCounter
// ===================================================================

TelemetryCounter::TelemetryCounter(std::string_view name)
    : name_(name) {
    TelemetrySystem::instance().register_counter(this);
}

TelemetryCounter::~TelemetryCounter() {
    TelemetrySystem::instance().deregister_counter(this);
}

std::int64_t TelemetryCounter::reset() {
    return value_.exchange(0, std::memory_order_acq_rel);
}

// ===================================================================
// TelemetryGauge
// ===================================================================

TelemetryGauge::TelemetryGauge(std::string_view name)
    : name_(name) {
    TelemetrySystem::instance().register_gauge(this);
}

TelemetryGauge::~TelemetryGauge() {
    TelemetrySystem::instance().deregister_gauge(this);
}

std::int64_t TelemetryGauge::reset() {
    return value_.exchange(0, std::memory_order_acq_rel);
}

// ===================================================================
// TelemetryHistogram
// ===================================================================

TelemetryHistogram::TelemetryHistogram(std::string_view name, std::vector<double> boundaries)
    : name_(name)
    , boundaries_(std::move(boundaries))
    , buckets_(boundaries_.size() + 1, 0) {
    TelemetrySystem::instance().register_histogram(this);
}

TelemetryHistogram::~TelemetryHistogram() {
    TelemetrySystem::instance().deregister_histogram(this);
}

void TelemetryHistogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Find the bucket index: first boundary that value < boundary
    auto it = std::upper_bound(boundaries_.begin(), boundaries_.end(), value);
    std::size_t idx = static_cast<std::size_t>(it - boundaries_.begin());
    if (idx >= buckets_.size()) idx = buckets_.size() - 1;
    buckets_[idx]++;
}

std::vector<std::int64_t> TelemetryHistogram::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto snap = buckets_;
    std::fill(buckets_.begin(), buckets_.end(), 0);
    return snap;
}

// ===================================================================
// TelemetrySystem
// ===================================================================

TelemetrySystem& TelemetrySystem::instance() {
    static TelemetrySystem sys;
    return sys;
}

void TelemetrySystem::register_counter(TelemetryCounter* counter) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.push_back(counter);
}

void TelemetrySystem::register_gauge(TelemetryGauge* gauge) {
    std::lock_guard<std::mutex> lock(mutex_);
    gauges_.push_back(gauge);
}

void TelemetrySystem::register_histogram(TelemetryHistogram* histogram) {
    std::lock_guard<std::mutex> lock(mutex_);
    histograms_.push_back(histogram);
}

TelemetrySnapshot TelemetrySystem::snapshot() {
    TelemetrySnapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);

    for (auto* c : counters_) {
        snap.counters.push_back({c->name(), c->reset()});
    }
    for (auto* g : gauges_) {
        snap.gauges.push_back({g->name(), g->get()});
    }
    for (auto* h : histograms_) {
        snap.histograms.push_back({h->name(), h->boundaries(), h->reset()});
    }

    return snap;
}

void TelemetrySystem::flush() {
    if (!enabled_) return;

    auto snap = snapshot();

    for (const auto& c : snap.counters) {
        if (c.value != 0) {
            log_info_cat(AE_LOG_CATEGORY, "counter," + c.name + "," + std::to_string(c.value));
        }
    }
    for (const auto& g : snap.gauges) {
        log_info_cat(AE_LOG_CATEGORY, "gauge," + g.name + "," + std::to_string(g.value));
    }
    for (const auto& h : snap.histograms) {
        std::string line = "histogram," + h.name;
        for (std::size_t i = 0; i < h.buckets.size(); ++i) {
            line += "," + (i < h.boundaries.size()
                               ? std::to_string(h.boundaries[i])
                               : "inf");
            line += ":" + std::to_string(h.buckets[i]);
        }
        log_info_cat(AE_LOG_CATEGORY, line);
    }
}

void TelemetrySystem::flush_to_csv(const std::string& path) {
    auto snap = snapshot();

    std::ofstream file(path);
    if (!file.is_open()) {
        log_warning_cat(AE_LOG_CATEGORY, "Cannot write CSV to " + path);
        return;
    }

    // Header
    file << "type,name,value" << "\n";

    for (const auto& c : snap.counters) {
        file << "counter," << c.name << "," << c.value << "\n";
    }
    for (const auto& g : snap.gauges) {
        file << "gauge," << g.name << "," << g.value << "\n";
    }
    for (const auto& h : snap.histograms) {
        for (std::size_t i = 0; i < h.buckets.size(); ++i) {
            std::string boundary = (i < h.boundaries.size())
                                       ? std::to_string(h.boundaries[i])
                                       : "inf";
            file << "histogram," << h.name << "_le_" << boundary << "," << h.buckets[i] << "\n";
        }
    }

    log_info_cat(AE_LOG_CATEGORY, "Telemetry CSV written to " + path);
}

void TelemetrySystem::deregister_counter(const TelemetryCounter* counter) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove(counters_.begin(), counters_.end(), counter);
    counters_.erase(it, counters_.end());
}

void TelemetrySystem::deregister_gauge(const TelemetryGauge* gauge) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove(gauges_.begin(), gauges_.end(), gauge);
    gauges_.erase(it, gauges_.end());
}

void TelemetrySystem::deregister_histogram(const TelemetryHistogram* histogram) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove(histograms_.begin(), histograms_.end(), histogram);
    histograms_.erase(it, histograms_.end());
}

void TelemetrySystem::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.clear();
    gauges_.clear();
    histograms_.clear();
}

}  // namespace ae

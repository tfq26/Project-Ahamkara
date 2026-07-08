#include "ae/core/frame_pacer.h"
#include "ae/core/time.h"

#include <algorithm>
#include <cmath>

namespace ae {

FramePacer::FramePacer(double budget_ms, double warn_threshold_ms)
    : budget_ms_(budget_ms)
    , warn_threshold_ms_(warn_threshold_ms) {}

void FramePacer::start_frame() {
    frame_start_time_ = now_seconds();
    frame_active_ = true;
}

void FramePacer::end_frame() {
    if (!frame_active_) return;
    const double now = now_seconds();
    const double delta_s = now - frame_start_time_;
    frame_active_ = false;
    end_frame(delta_s * 1000.0);
}

void FramePacer::end_frame(double frame_delta_ms) {
    ++frame_count_;
    raw_ms_ = frame_delta_ms;

    // Push into ring buffer
    history_[static_cast<std::size_t>(history_index_)] = frame_delta_ms;
    history_index_ = (history_index_ + 1) % kHistorySize;
    if (history_count_ < kHistorySize) ++history_count_;

    // Exponential moving average (α = 0.1)
    static constexpr double kAlpha = 0.1;
    if (frame_count_ == 1) {
        smooth_ms_ = frame_delta_ms;
    } else {
        smooth_ms_ = (1.0 - kAlpha) * smooth_ms_ + kAlpha * frame_delta_ms;
    }

    // Rolling average
    const int n = history_count_;
    if (n > 0) {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            sum += history_[static_cast<std::size_t>(i)];
        }
        rolling_avg_ms_ = sum / static_cast<double>(n);
    }

    // Recompute percentiles (every frame while history is small, then every 30)
    if (frame_count_ < kHistorySize || (frame_count_ % 30) == 0) {
        recompute_percentiles();
    }

    update_compliance();

    // Regression: smooth avg exceeds budget by threshold
    regression_ = (smooth_ms_ > budget_ms_ + warn_threshold_ms_);
}

void FramePacer::recompute_percentiles() {
    const int n = history_count_;
    if (n == 0) return;

    // Copy and sort for percentile (cheap for 200 items, not every frame)
    // We guard this in end_frame to only run periodically once warm.
    std::array<double, kHistorySize> sorted{};
    for (int i = 0; i < n; ++i) {
        sorted[static_cast<std::size_t>(i)] = history_[static_cast<std::size_t>(i)];
    }
    std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(n));

    // 1% low = average frame time of the slowest 1% of frames
    const int n_worst = std::max(1, static_cast<int>(0.01 * static_cast<double>(n)));
    double sum = 0.0;
    for (int i = n - n_worst; i < n; ++i) {
        sum += sorted[static_cast<std::size_t>(i)];
    }
    p01_low_ms_ = sum / static_cast<double>(n_worst);
}

void FramePacer::update_compliance() {
    const int n = history_count_;
    if (n == 0) {
        compliance_ = 1.0;
        return;
    }

    int within = 0;
    for (int i = 0; i < n; ++i) {
        if (history_[static_cast<std::size_t>(i)] <= budget_ms_) {
            ++within;
        }
    }
    compliance_ = static_cast<double>(within) / static_cast<double>(n);
}

void FramePacer::reset() {
    history_.fill(0.0);
    history_index_ = 0;
    history_count_ = 0;
    frame_count_ = 0;
    raw_ms_ = 0.0;
    smooth_ms_ = 0.0;
    rolling_avg_ms_ = 0.0;
    p01_low_ms_ = 0.0;
    compliance_ = 1.0;
    regression_ = false;
    frame_start_time_ = 0.0;
    frame_active_ = false;
}

}  // namespace ae

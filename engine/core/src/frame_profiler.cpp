#include "ae/core/frame_profiler.h"

#include <algorithm>
#include <cmath>

namespace ae {

// =============================================================================
// FrameProfiler
// =============================================================================

FrameProfiler::FrameProfiler()
    : frame_start_(std::chrono::steady_clock::now()) {}

void FrameProfiler::begin_section(ProfileSection section) {
    const auto idx = static_cast<std::size_t>(section);
    // If the section is already active (nested without matching end), do nothing.
    if (sections_[idx].active)
        return;
    sections_[idx].active = true;
    sections_[idx].section_start = std::chrono::steady_clock::now();
}

void FrameProfiler::end_section(ProfileSection section) {
    const auto idx = static_cast<std::size_t>(section);
    if (!sections_[idx].active)
        return;

    const auto now = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(now - sections_[idx].section_start).count();

    sections_[idx].active = false;
    sections_[idx].frame_accum_ms += elapsed_ms;
    sections_[idx].call_count++;
}

FrameProfileSnapshot FrameProfiler::end_frame(std::uint64_t frame_number) {
    FrameProfileSnapshot snapshot;
    snapshot.frame_number = frame_number;

    const auto now = std::chrono::steady_clock::now();
    snapshot.frame_total_ms =
        std::chrono::duration<double, std::milli>(now - frame_start_).count();
    frame_start_ = now;

    constexpr double kAlpha = 0.1;
    constexpr double kEmaInitAlpha = 0.3; // faster initial ramp

    for (std::size_t i = 0; i < kSectionCount; ++i) {
        auto& s = sections_[i];
        const double current = s.frame_accum_ms;

        snapshot.sections[i].current_ms = current;
        snapshot.sections[i].call_count = s.call_count;

        if (current > 0.0) {
            // Update min/max
            if (total_frames_ == 0 || current < s.min_ms)
                s.min_ms = current;
            if (total_frames_ == 0 || current > s.max_ms)
                s.max_ms = current;

            // Update EMA
            if (total_frames_ == 0) {
                s.smooth_ms = current;
            } else if (total_frames_ < 10) {
                // Faster ramp for the first 10 frames
                s.smooth_ms = (1.0 - kEmaInitAlpha) * s.smooth_ms + kEmaInitAlpha * current;
            } else {
                s.smooth_ms = (1.0 - kAlpha) * s.smooth_ms + kAlpha * current;
            }
        }

        snapshot.sections[i].min_ms = s.min_ms;
        snapshot.sections[i].max_ms = s.max_ms;
        snapshot.sections[i].avg_ms = s.smooth_ms;

        // Reset per-frame accumulators
        s.frame_accum_ms = 0.0;
        s.call_count = 0;
    }

    ++total_frames_;
    return snapshot;
}

void FrameProfiler::reset() {
    for (auto& s : sections_) {
        s.frame_accum_ms = 0.0;
        s.min_ms = 0.0;
        s.max_ms = 0.0;
        s.smooth_ms = 0.0;
        s.call_count = 0;
        s.active = false;
    }
    frame_start_ = std::chrono::steady_clock::now();
    total_frames_ = 0;
}

// =============================================================================
// ScopedProfile — RAII
// =============================================================================

FrameProfiler::ScopedProfile::ScopedProfile(FrameProfiler& profiler, ProfileSection section)
    : profiler_(profiler), section_(section) {
    profiler_.begin_section(section_);
}

FrameProfiler::ScopedProfile::~ScopedProfile() {
    profiler_.end_section(section_);
}

} // namespace ae

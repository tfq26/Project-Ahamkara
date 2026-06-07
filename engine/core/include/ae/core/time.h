#pragma once

#include <chrono>

namespace ae {

double now_seconds();

/**
 * @brief Compute elapsed seconds since the previous frame and update
 *        the previous-frame timestamp in-place.
 *
 * Convenience helper used by both the dedicated server and headless
 * client entrypoints so they do not need to duplicate this logic.
 */
inline float compute_frame_dt(std::chrono::steady_clock::time_point& previous) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - previous).count();
    previous = now;
    return dt;
}

}  // namespace ae


#include "ae/network/network_clock.h"

#include <algorithm>
#include <cmath>

namespace ae {

void NetworkClock::record_snapshot(u32 server_tick, float tick_rate_hz, double local_time_s) {
    const float server_time = static_cast<float>(server_tick) / tick_rate_hz;
    const float local_time = static_cast<float>(local_time_s);
    const float raw_offset = server_time - local_time;

    if (!initialized_) {
        offset_seconds_ = raw_offset;
        initialized_ = true;
    } else {
        // Exponentially-weighted moving average to smooth jitter.
        offset_seconds_ = offset_seconds_ * (1.0F - kSmoothingAlpha)
                        + raw_offset * kSmoothingAlpha;
    }
}

float NetworkClock::estimate_server_time(double local_time_s, float tick_rate_hz) const {
    if (!initialized_) {
        return static_cast<float>(local_time_s);
    }

    const float projected = static_cast<float>(local_time_s) + offset_seconds_;
    return projected;
}

void NetworkClock::record_rtt(float rtt_seconds) {
    if (rtt_seconds <= 0.0F) {
        return;
    }

    if (rtt_seconds_ <= 0.0F) {
        rtt_seconds_ = rtt_seconds;
    } else {
        rtt_seconds_ = rtt_seconds_ * (1.0F - kRttSmoothingAlpha)
                     + rtt_seconds * kRttSmoothingAlpha;
    }
}

void NetworkClock::reset() {
    offset_seconds_ = 0.0F;
    rtt_seconds_ = 0.0F;
    initialized_ = false;
}

}  // namespace ae

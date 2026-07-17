#include "ahamkara/game/rewind.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ahamkara::game {

// -----------------------------------------------------------------------
// ServerClockTracker
// -----------------------------------------------------------------------

ae::u32 ServerClockTracker::convert(ae::u32 client_tick, float /*client_rtt*/) const {
    if (latest_server_tick_ == 0) {
        // No server ticks recorded yet — cannot convert.
        return std::numeric_limits<ae::u32>::max();
    }

    // The client cannot have seen anything after the latest server tick
    // (the server hasn't sent it yet).  Clamp to latest.
    if (client_tick > latest_server_tick_) {
        return latest_server_tick_;
    }

    // Clamp the rewind so it does not exceed the maximum window.
    ae::u32 rewind_ticks = latest_server_tick_ - client_tick;
    if (rewind_ticks > kMaxRewindTicks) {
        return latest_server_tick_ - kMaxRewindTicks;
    }

    return client_tick;
}

HitReject ServerClockTracker::validate_rewind(ae::u32 client_tick,
                                              ae::u32 server_tick) const {
    if (latest_server_tick_ == 0) {
        return HitReject::NoHistory;
    }

    // Future tick: client says they fired at a time beyond the server's
    // current tick.  This is impossible without client-side time manipulation.
    if (client_tick > latest_server_tick_ + 1) {
        return HitReject::FutureTick;
    }

    // Expired: the calculated server tick is older than history retention.
    // The history buffer is 1024 entries.  We consider anything more than
    // that minus a small margin as expired.
    constexpr ae::u32 kRetentionTicks = 1024;
    if (server_tick + kRetentionTicks < latest_server_tick_) {
        return HitReject::Expired;
    }

    // Out-of-window: the rewind delta exceeds the maximum allowed.
    if (latest_server_tick_ > client_tick) {
        ae::u32 rewind_delta = latest_server_tick_ - client_tick;
        if (rewind_delta > kMaxRewindTicks) {
            return HitReject::OutOfWindow;
        }
    }

    return HitReject::None;
}

// -----------------------------------------------------------------------
// RewindValidation
// -----------------------------------------------------------------------

RewindValidation::RewindValidation(
    const ae::ServerHistoryBuffer<HistoricalState, 1024>& history)
    : history_(history) {}

HitResult RewindValidation::validate_hit(
    ae::u32 server_tick,
    const Vec3& origin,
    const Vec3& forward,
    float base_damage,
    float headshot_multiplier) const {

    HitResult result {};

    // Query the history buffer for the requested tick.
    HistoricalState hist {};
    if (!history_.get(server_tick, hist)) {
        result.reject_reason = HitReject::NoHistory;
        return result;
    }

    Vec3 ray_end {
        origin.x + forward.x * kHitscanRange,
        origin.y + forward.y * kHitscanRange,
        origin.z + forward.z * kHitscanRange};

    float closest_t = kHitscanRange;
    bool hit_something = false;
    bool is_headshot = false;
    int hit_dummy_idx = -1;
    Vec3 hit_position = ray_end;

    // Ray direction (normalised).
    Vec3 ray_dir {forward};
    float ray_len = std::sqrt(ray_dir.x * ray_dir.x +
                              ray_dir.y * ray_dir.y +
                              ray_dir.z * ray_dir.z);
    if (ray_len > 0.001F) {
        ray_dir.x /= ray_len;
        ray_dir.y /= ray_len;
        ray_dir.z /= ray_len;
    } else {
        result.reject_reason = HitReject::NoHistory;
        return result;
    }

    // Test each dummy at its historical position (lag compensation).
    // We use the dummy_id from hist as the stable identity.
    for (int i = 0; i < HistoricalState::kMaxDummies; ++i) {
        if (!hist.dummy_alive[i])
            continue;

        const Vec3 d_pos = hist.dummy_positions[i];
        const Vec3 d_bottom = {d_pos.x, d_pos.y - kDummyHalfHeight, d_pos.z};
        const Vec3 d_top = {d_pos.x, d_pos.y + kDummyHalfHeight, d_pos.z};

        // Ray-vs-infinite-cylinder test (XZ plane).
        Vec3 oc = {origin.x - d_pos.x, origin.y - d_pos.y, origin.z - d_pos.z};
        float a = ray_dir.x * ray_dir.x + ray_dir.z * ray_dir.z;
        if (a < 0.001F)
            continue; // Ray is near-vertical — skip cylinder.
        float b = 2.0F * (oc.x * ray_dir.x + oc.z * ray_dir.z);
        float c = oc.x * oc.x + oc.z * oc.z - kDummyRadius * kDummyRadius;
        float disc = b * b - 4.0F * a * c;

        if (disc >= 0.0F) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-b - sqrt_disc) / (2.0F * a);
            float t1 = (-b + sqrt_disc) / (2.0F * a);
            if (t0 > t1)
                std::swap(t0, t1);

            // Test cylinder body.
            for (float t : {t0, t1}) {
                if (t > 0.001F && t < closest_t) {
                    float hit_y = origin.y + ray_dir.y * t;
                    if (hit_y >= d_bottom.y && hit_y <= d_top.y) {
                        closest_t = t;
                        hit_something = true;
                        hit_dummy_idx = i;
                        hit_position = {origin.x + ray_dir.x * t, hit_y,
                                        origin.z + ray_dir.z * t};
                        is_headshot = (hit_y >= d_top.y - 0.3F);
                    }
                }
            }

            // Hemisphere caps (approximate as full-sphere test for top and
            // bottom centres).
            for (const Vec3& cap_center : {d_top, d_bottom}) {
                Vec3 to_cap = {origin.x - cap_center.x,
                               origin.y - cap_center.y,
                               origin.z - cap_center.z};
                float cap_b = 2.0F * (to_cap.x * ray_dir.x +
                                      to_cap.y * ray_dir.y +
                                      to_cap.z * ray_dir.z);
                float cap_c = to_cap.x * to_cap.x +
                              to_cap.y * to_cap.y +
                              to_cap.z * to_cap.z - kDummyRadius * kDummyRadius;
                float cap_disc = cap_b * cap_b - 4.0F * cap_c;
                if (cap_disc >= 0.0F) {
                    float cap_t = (-cap_b - std::sqrt(cap_disc)) * 0.5F;
                    if (cap_t > 0.001F && cap_t < closest_t) {
                        closest_t = cap_t;
                        hit_something = true;
                        is_headshot = (&cap_center == &d_top);
                        hit_dummy_idx = i;
                        hit_position = {origin.x + ray_dir.x * cap_t,
                                        origin.y + ray_dir.y * cap_t,
                                        origin.z + ray_dir.z * cap_t};
                    }
                }
            }
        }
    }

    if (!hit_something) {
        return result; // miss, reject_reason stays None, hit stays false.
    }

    float damage = base_damage;
    if (is_headshot)
        damage *= headshot_multiplier;

    result.hit = true;
    result.hit_dummy_idx = hit_dummy_idx;
    result.hit_position = hit_position;
    result.damage = damage;
    result.is_headshot = is_headshot;
    return result;
}

void RewindValidation::mark_processed(ae::u32 client_tick, int dummy_idx) {
    ae::u32 key = (client_tick << 16) | (static_cast<ae::u32>(dummy_idx) & 0xFFFF);
    processed_hits_.insert(key);
}

void RewindValidation::clear_processed() {
    processed_hits_.clear();
}

} // namespace ahamkara::game

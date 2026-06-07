#pragma once

#include "ae/core/types.h"

#include <cstdint>

namespace ae {

/**
 * @brief Deterministic pseudo-random number generator (Xorshift64).
 *
 * Seed is an explicit u64. Given the same seed, produces identical
 * sequences across all platforms. Used for gameplay logic that must
 * be deterministic across client and server.
 *
 * NOT suitable for cryptography. NOT suitable for non-deterministic
 * visual effects (use std::mt19937 or similar for those).
 */
class DeterministicRng {
public:
    explicit DeterministicRng(u64 seed = 123456789ULL) : state_(seed | 1ULL) {}

    /** Set the seed explicitly (e.g. for server-authoritative seed sync). */
    void seed(u64 s) { state_ = s | 1ULL; }

    /** Get the current raw state (for save/restore or network sync). */
    [[nodiscard]] u64 state() const { return state_; }

    /** Generate a random u64. */
    [[nodiscard]] u64 next_u64() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return state_;
    }

    /** Generate a random u32. */
    [[nodiscard]] u32 next_u32() {
        return static_cast<u32>(next_u64() >> 32);
    }

    /** Generate a random float in [0, 1). */
    [[nodiscard]] float next_f32() {
        // Use top 23 bits for mantissa
        return static_cast<float>(next_u32() & 0x007FFFFFu) / 8388608.0F;
    }

    /** Generate a random float in [min, max). */
    [[nodiscard]] float next_f32_range(float min, float max) {
        return min + next_f32() * (max - min);
    }

    /** Generate a random i32 in [min, max] inclusive. */
    [[nodiscard]] i32 next_i32_range(i32 min, i32 max) {
        u32 range = static_cast<u32>(max - min + 1);
        return min + static_cast<i32>(next_u32() % range);
    }

private:
    u64 state_;
};

/**
 * @brief Fixed-timestep simulation accumulator.
 *
 * Usage pattern (in game loop):
 * @code
 *   FixedTimestepAccumulator acc(1.0 / 60.0); // 60 Hz sim
 *   while (running) {
 *       double dt = measure_frame_time();
 *       acc.accumulate(dt);
 *       while (acc.consume()) {
 *           world.tick(acc.step());  // deterministic fixed-step update
 *       }
 *       double alpha = acc.interpolation_alpha();
 *       render(alpha);  // interpolated render state
 *   }
 * @endcode
 *
 * This decouples the simulation rate from the render rate.
 * The simulation always runs at `step()` seconds per tick,
 * regardless of frame rate. The renderer receives an
 * `interpolation_alpha()` value [0, 1] for smooth visual
 * interpolation between simulation states.
 */
class FixedTimestepAccumulator {
public:
    /**
     * @param step_seconds  Fixed simulation timestep (e.g. 1.0/60.0).
     * @param max_steps     Maximum steps to consume per frame (prevents spiral of death).
     */
    explicit FixedTimestepAccumulator(double step_seconds, int max_steps = 8)
        : step_seconds_(step_seconds), max_steps_(max_steps) {}

    /** Add frame delta time to the accumulator. */
    void accumulate(double delta_seconds) {
        accumulator_ += delta_seconds;
    }

    /** True if another fixed step is ready to be consumed. */
    [[nodiscard]] bool can_consume() const {
        return accumulator_ >= step_seconds_;
    }

    /** Consume one fixed step. Returns false if no step is ready. */
    bool consume() {
        if (accumulator_ < step_seconds_) return false;
        accumulator_ -= step_seconds_;
        ++steps_consumed_this_frame_;
        return true;
    }

    /** Get the fixed timestep duration. */
    [[nodiscard]] double step() const { return step_seconds_; }

    /**
     * @brief Get interpolation alpha for rendering.
     *
     * @return Value in [0, 1] representing how far between the previous
     *         and current simulation state the renderer should interpolate.
     *         Clamped to 1.0 when no more steps are pending.
     */
    [[nodiscard]] float interpolation_alpha() const {
        if (step_seconds_ <= 0.0) return 1.0F;
        float alpha = static_cast<float>(accumulator_ / step_seconds_);
        if (alpha > 1.0F) alpha = 1.0F;
        return alpha;
    }

    /** Reset the accumulator (e.g. after a large pause or world load). */
    void reset() {
        accumulator_ = 0.0;
        steps_consumed_this_frame_ = 0;
    }

    /** Number of steps consumed in the current frame. */
    [[nodiscard]] int steps_this_frame() const { return steps_consumed_this_frame_; }

    /** Maximum steps per frame before we start dropping (spiral-of-death guard). */
    [[nodiscard]] int max_steps() const { return max_steps_; }

    /** Reset the per-frame step counter (call at start of each frame). */
    void begin_frame() { steps_consumed_this_frame_ = 0; }

private:
    double step_seconds_;
    double accumulator_ {0.0};
    int max_steps_;
    int steps_consumed_this_frame_ {0};
};

}  // namespace ae

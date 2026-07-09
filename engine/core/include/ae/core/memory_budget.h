#pragma once

#include <cstddef>
#include <cstdint>

namespace ae {

class FrameAllocator;

/**
 * @brief Memory budget tracker with soft/hard thresholds.
 *
 * Tracks process RSS (resident set size) and optionally a FrameAllocator's
 * peak usage against configurable soft and hard budget limits.
 *
 * Pressure levels:
 *   - Ok:     below soft limit
 *   - Warning: soft limit exceeded (approaching OOM)
 *   - Critical: hard limit exceeded (action required)
 *
 * Usage:
 * @code
 *   MemoryBudgetTracker budget(512.0 * 1024.0 * 1024.0,  // 512 MB soft
 *                               768.0 * 1024.0 * 1024.0); // 768 MB hard
 *   budget.update();  // reads current RSS
 *   budget.track_frame_allocator(allocator);
 *
 *   if (budget.rss_pressure() == MemoryBudgetTracker::Pressure::Critical) {
 *       log_warning("RSS exceeded hard budget!");
 *   }
 * @endcode
 */
class MemoryBudgetTracker {
public:
    /// Budget pressure level.
    enum class Pressure : std::uint8_t {
        Ok = 0,
        Warning,
        Critical,
    };

    /**
     * @param rss_soft_bytes  Soft budget for RSS. 0 = disabled.
     * @param rss_hard_bytes  Hard budget for RSS. 0 = disabled.
     * @param alloc_soft_bytes  Soft budget for frame allocator peak. 0 = disabled.
     * @param alloc_hard_bytes  Hard budget for frame allocator peak. 0 = disabled.
     */
    explicit MemoryBudgetTracker(
        std::size_t rss_soft_bytes = 512ULL * 1024ULL * 1024ULL,   // 512 MB
        std::size_t rss_hard_bytes = 768ULL * 1024ULL * 1024ULL,   // 768 MB
        std::size_t alloc_soft_bytes = 0,
        std::size_t alloc_hard_bytes = 0);

    /// Update RSS reading from the OS. Call once per frame.
    void update_rss();

    /// Track a FrameAllocator's peak usage against allocator budgets.
    void track_frame_allocator(const FrameAllocator& alloc);

    // --- Accessors ---

    /// Current RSS in bytes.
    [[nodiscard]] std::size_t rss_bytes() const { return rss_bytes_; }

    /// Peak RSS observed since construction or reset.
    [[nodiscard]] std::size_t peak_rss_bytes() const { return peak_rss_bytes_; }

    /// Current frame allocator peak usage (last tracked value).
    [[nodiscard]] std::size_t alloc_peak_bytes() const { return alloc_peak_bytes_; }

    /// RSS pressure level.
    [[nodiscard]] Pressure rss_pressure() const { return rss_pressure_; }

    /// Frame allocator pressure level.
    [[nodiscard]] Pressure alloc_pressure() const { return alloc_pressure_; }

    /// RSS soft budget in bytes.
    [[nodiscard]] std::size_t rss_soft_bytes() const { return rss_soft_bytes_; }

    /// RSS hard budget in bytes.
    [[nodiscard]] std::size_t rss_hard_bytes() const { return rss_hard_bytes_; }

    /// Set RSS soft budget dynamically.
    void set_rss_soft_bytes(std::size_t bytes) { rss_soft_bytes_ = bytes; }

    /// Set RSS hard budget dynamically.
    void set_rss_hard_bytes(std::size_t bytes) { rss_hard_bytes_ = bytes; }

    /// Set allocator soft budget dynamically.
    void set_alloc_soft_bytes(std::size_t bytes) { alloc_soft_bytes_ = bytes; }

    /// Set allocator hard budget dynamically.
    void set_alloc_hard_bytes(std::size_t bytes) { alloc_hard_bytes_ = bytes; }

    /// Reset peak RSS tracker.
    void reset();

private:
    Pressure compute_pressure(std::size_t value,
                              std::size_t soft, std::size_t hard) const;

    std::size_t rss_soft_bytes_;
    std::size_t rss_hard_bytes_;
    std::size_t alloc_soft_bytes_;
    std::size_t alloc_hard_bytes_;

    std::size_t rss_bytes_{0};
    std::size_t peak_rss_bytes_{0};
    std::size_t alloc_peak_bytes_{0};

    Pressure rss_pressure_{Pressure::Ok};
    Pressure alloc_pressure_{Pressure::Ok};
};

}  // namespace ae

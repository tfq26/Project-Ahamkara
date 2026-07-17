#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ae {

// =============================================================================
// Predefined profiling sections — one per major engine subsystem.
// =============================================================================

enum class ProfileSection : std::uint8_t {
    Render = 0,
    Physics,
    Animation,
    Audio,
    UI,
    Simulation,
    Network,
    Other,
    Count, // sentinel — must be last
};

/// Human-readable name for each section. The array is indexed by ProfileSection.
constexpr std::string_view kProfileSectionNames[] = {
    "Render",
    "Physics",
    "Animation",
    "Audio",
    "UI",
    "Simulation",
    "Network",
    "Other",
};
static_assert(std::size(kProfileSectionNames) == static_cast<std::size_t>(ProfileSection::Count));

// =============================================================================
// Per-section timing data for a single frame.
// =============================================================================

struct ProfileSectionData {
    double current_ms {0.0};      ///< Accumulated time for this section in the sampled frame
    double min_ms {0.0};          ///< Minimum over the accumulation window
    double max_ms {0.0};          ///< Maximum over the accumulation window
    double avg_ms {0.0};          ///< Moving average over the accumulation window
    std::uint64_t call_count {0}; ///< How many times the section was entered
};

// =============================================================================
// Per-frame aggregate snapshot — combines profiler sections + frame info.
// =============================================================================

struct FrameProfileSnapshot {
    static constexpr std::size_t kSectionCount =
        static_cast<std::size_t>(ProfileSection::Count);

    ProfileSectionData sections[kSectionCount] {};
    double frame_total_ms {0.0};
    std::uint64_t frame_number {0};
};

// =============================================================================
// FrameProfiler — lightweight RAII-based CPU profiling for named sections.
//
// NOT thread-safe for the same FrameProfiler instance. Designed to be owned by
// the main loop. Multiple sections within the same frame nest correctly as
// long as begin/end pairs are properly ordered (stack discipline).
//
// Usage:
//   FrameProfiler profiler;
//
//   void game_loop() {
//       {
//           ScopedProfile _sp(profiler, ProfileSection::Physics);
//           physics_world_.step(dt);
//       }
//       {
//           ScopedProfile _sp(profiler, ProfileSection::Render);
//           renderer_.render();
//       }
//       auto snapshot = profiler.end_frame(frame_count);
//       // use snapshot.sections[...] for display / logging
//   }
// =============================================================================

class FrameProfiler {
  public:
    static constexpr std::size_t kSectionCount =
        static_cast<std::size_t>(ProfileSection::Count);

    FrameProfiler();

    /// Start timing a section.  Must be paired with end_section().
    void begin_section(ProfileSection section);

    /// Stop timing a section.  Accumulates elapsed time.
    void end_section(ProfileSection section);

    // =====================================================================
    // RAII helper — records time from construction to destruction.
    // =====================================================================
    class ScopedProfile {
      public:
        ScopedProfile(FrameProfiler& profiler, ProfileSection section);
        ~ScopedProfile();

        ScopedProfile(const ScopedProfile&) = delete;
        ScopedProfile& operator=(const ScopedProfile&) = delete;
        ScopedProfile(ScopedProfile&&) = delete;
        ScopedProfile& operator=(ScopedProfile&&) = delete;

      private:
        FrameProfiler& profiler_;
        ProfileSection section_;
    };

    /// End the current frame: compute section snapshots, update min/max/avg,
    /// reset per-frame accumulators, and return a FrameProfileSnapshot.
    [[nodiscard]] FrameProfileSnapshot end_frame(std::uint64_t frame_number);

    /// Reset all accumulated history (min/max/avg and per-frame accumulators).
    void reset();

  private:
    // One state block per section; cache-line-aligned to prevent false sharing.
    struct alignas(64) SectionState {
        /// Accumulated ms in the current frame (written by end_section).
        double frame_accum_ms {0.0};
        /// Running minimum over the smoothing window.
        double min_ms {0.0};
        /// Running maximum over the smoothing window.
        double max_ms {0.0};
        /// Exponential moving average (alpha = 0.1).
        double smooth_ms {0.0};
        /// Number of times this section was entered in the current frame.
        std::uint64_t call_count {0};
        /// Whether the section is currently being timed (for nesting check).
        bool active {false};
        /// Timestamp captured by begin_section.
        std::chrono::steady_clock::time_point section_start {};
    };

    std::array<SectionState, kSectionCount> sections_ {};

    // Frame start wall-clock time.
    std::chrono::steady_clock::time_point frame_start_;

    /// Number of frames accumulated so far (for initialising EMA).
    std::uint64_t total_frames_ {0};
};

} // namespace ae

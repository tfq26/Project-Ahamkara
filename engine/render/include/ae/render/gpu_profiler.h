#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ae::render {

// =============================================================================
// Named GPU profiling sections, mirroring the CPU-side ProfileSection concept.
// =============================================================================

enum class GpuProfileSection : std::uint8_t {
    Frame = 0, // total GPU frame time (between swap or present)
    Shadow,
    Opaque,
    Transparent,
    PostProcess,
    UI,
    Count,
};

constexpr std::string_view kGpuSectionNames[] = {
    "Frame",
    "Shadow",
    "Opaque",
    "Transparent",
    "PostProcess",
    "UI",
};
static_assert(std::size(kGpuSectionNames) == static_cast<std::size_t>(GpuProfileSection::Count));

// =============================================================================
// Per-section GPU timing result (latched from previous frame's queries).
// =============================================================================

struct GpuSectionData {
    double elapsed_ms {0.0}; ///< GPU time in ms (may be 0 if query not ready)
    bool available {false};  ///< True when the GPU timestamp data is valid
};

// =============================================================================
// GpuProfiler — lightweight wrapper around GL_TIMESTAMP queries.
//
// Uses GL_QUERY_COUNTER (timestamp) pairs to measure GPU-side elapsed time for
// named sections. Queries are double-buffered so we never stall: the current
// frame issues queries, and the previous frame's results are read back.
//
// Thread-safety: NOT thread-safe. Owned by the render thread.
//
// Usage:
//   GpuProfiler gpu_prof;
//   gpu_prof.init(max_sections);
//
//   void render_frame() {
//       gpu_prof.begin_frame();
//       gpu_prof.begin_section(GpuProfileSection::Opaque);
//       // ... draw opaque geometry ...
//       gpu_prof.end_section(GpuProfileSection::Opaque);
//       gpu_prof.end_frame();
//
//       auto results = gpu_prof.read_results();
//       for (const auto& r : results) { ... }
//   }
// =============================================================================

class GpuProfiler {
  public:
    static constexpr std::size_t kSectionCount =
        static_cast<std::size_t>(GpuProfileSection::Count);

    // Two query objects per section per frame (begin + end).
    static constexpr int kQueriesPerSection = 2;
    static constexpr int kQueryRingSize = 3; // triple-buffered

    GpuProfiler();
    ~GpuProfiler();

    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    /// Initialize GL query objects. Must be called on the render thread with a
    /// current GL context. Returns false if GL timestamps are not available.
    bool init();

    /// Release all GL query objects.
    void shutdown();

    /// Begin GPU profiling for a section. Must be called within begin_frame() /
    /// end_frame().
    void begin_section(GpuProfileSection section);

    /// End a GPU profiling section.
    void end_section(GpuProfileSection section);

    /// Start a new profiling frame: advance query ring index.
    void begin_frame();

    /// End the profiling frame.
    void end_frame();

    /// Read results from the oldest completed frame. Returns an array indexed
    /// by GpuProfileSection. Sections with no data or unavailable queries
    /// report available=false.
    [[nodiscard]] std::array<GpuSectionData, kSectionCount> read_results();

    /// True if GL timer queries are supported.
    [[nodiscard]] bool supported() const {
        return supported_;
    }

  private:
    bool supported_ {false};
    int current_ring_ {0};

    // Query object IDs: [ring_slot][section][0=begin, 1=end]
    std::array<
        std::array<unsigned int, kSectionCount * kQueriesPerSection>,
        kQueryRingSize>
        query_ids_ {};
};

} // namespace ae::render

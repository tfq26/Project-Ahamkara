#pragma once

#include "ae/render/pbr_renderer.h"
#include "ae/render/render_backend.h"

#include <cstdint>
#include <vector>

namespace ae::render {

/// Manages a pool of reflection probes with spatial queries.
/// Each probe is stored alongside a cached AABB for proximity tests.
class ReflectionProbeManager {
  public:
    ReflectionProbeManager() = default;
    ~ReflectionProbeManager() = default;

    ReflectionProbeManager(const ReflectionProbeManager&) = delete;
    ReflectionProbeManager& operator=(const ReflectionProbeManager&) = delete;

    /// Add a probe.  AABB is computed automatically from position/radius.
    /// Returns the index of the new probe.
    std::size_t add_probe(const ReflectionProbe& probe);

    /// Remove a probe by index.  Invalidates subsequent indices.
    void remove_probe(std::size_t index);

    /// Remove all probes.
    void clear();

    /// Find probe indices whose AABB contains the given position.
    std::vector<std::size_t> find_probes_near(const float position[3]) const;

    /// Access probe by index (const).
    const ReflectionProbe& get_probe(std::size_t index) const;

    /// Number of probes in the pool.
    std::size_t count() const {
        return entries_.size();
    }

  private:
    struct ProbeEntry {
        ReflectionProbe probe;
        float aabb_min[3] = {};
        float aabb_max[3] = {};
    };

    std::vector<ProbeEntry> entries_;
};

// ── Inline implementations ──────────────────────────────────────────────────

inline std::size_t ReflectionProbeManager::add_probe(const ReflectionProbe& probe) {
    ProbeEntry entry;
    entry.probe = probe;
    for (int i = 0; i < 3; ++i) {
        entry.aabb_min[i] = probe.position[i] - probe.influence_radius;
        entry.aabb_max[i] = probe.position[i] + probe.influence_radius;
    }
    entries_.push_back(entry);
    return entries_.size() - 1;
}

inline void ReflectionProbeManager::remove_probe(std::size_t index) {
    if (index < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

inline void ReflectionProbeManager::clear() {
    entries_.clear();
}

inline std::vector<std::size_t> ReflectionProbeManager::find_probes_near(
    const float position[3]) const {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        bool inside = true;
        for (int axis = 0; axis < 3; ++axis) {
            if (position[axis] < e.aabb_min[axis] ||
                position[axis] > e.aabb_max[axis]) {
                inside = false;
                break;
            }
        }
        if (inside) {
            result.push_back(i);
        }
    }
    return result;
}

inline const ReflectionProbe& ReflectionProbeManager::get_probe(std::size_t index) const {
    return entries_[index].probe;
}

} // namespace ae::render

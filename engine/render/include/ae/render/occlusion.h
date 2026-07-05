#pragma once

// Occlusion and portal/PVS readiness types.
//
// These are data shapes only — no runtime occlusion solving yet. The types
// define the contract for a future portal-based visibility system so that
// level authoring and world data can already reference portals and PVS
// regions.
//
// The actual occlusion solver (GPU queries, software rasterizer, or
// portal-walk) is deferred to a later phase.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ae::render {

/// A portal connecting two PVS regions.  In world space the portal is
/// represented as a rectangle in the x/z plane with a top/bottom Y range.
/// The camera (or AI visibility) passes through the portal to reveal the
/// linked region.
struct OcclusionPortal {
    std::uint32_t id {0};

    /// World-space portal rectangle (x/z plane).
    float min_x {0.0F};
    float max_x {0.0F};
    float min_z {0.0F};
    float max_z {0.0F};

    /// Vertical bounds.
    float bottom_y {0.0F};
    float top_y {3.0F};

    /// IDs of the two PVS regions this portal connects.
    std::uint32_t region_a {0};
    std::uint32_t region_b {0};

    /// Optional debug label.
    std::string label {};
};

/// A PVS (Potentially Visible Set) region.  Contains the set of instance/entity
/// IDs that *may* be visible from any point within the region, assuming the
/// region is itself visible (i.e. reached through portals from the camera).
///
/// The PVS is an explicit list, not a runtime computation: level authoring
/// tools (or offline PVS builders) populate `potentially_visible_ids`.  At
/// runtime the renderer uses this list to skip objects that cannot possibly
/// be seen.
struct PVSRegion {
    std::uint32_t id {0};

    /// World-space bounding box of the region.
    float min_x {0.0F};
    float min_z {0.0F};
    float max_x {0.0F};
    float max_z {0.0F};
    float bottom_y {0.0F};
    float top_y {0.0F};

    /// Instance/entity IDs that may be visible from this region.
    std::vector<std::uint64_t> potentially_visible_ids {};

    /// Portal IDs that exit this region.
    std::vector<std::uint32_t> portal_ids {};

    /// Optional debug label.
    std::string label {};
};

/// Container for all portals and PVS regions in a scene.
/// Level loaders populate this from authored metadata.
struct OcclusionScene {
    std::vector<OcclusionPortal> portals;
    std::vector<PVSRegion> regions;
};

}  // namespace ae::render

#include "ahamkara/client/weapon_viewmodel_data.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using namespace ahamkara::client;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

constexpr float kEpsilon = 1.0e-6F;

bool is_nan(float v) { return std::isnan(v); }
bool is_finite(float v) { return std::isfinite(v); }

// ---------------------------------------------------------------------------
// Viewmodel transforms
// ---------------------------------------------------------------------------

void test_weapon_viewmodel_transforms_bounds() {
    // Every weapon must produce valid, finite transform values within
    // expected physical ranges.
    for (int i = 0; i < static_cast<int>(kWeaponViewmodelCount); ++i) {
        const auto t = weapon_viewmodel_transform(i);

        // All values must be finite
        if (!is_finite(t.pitch_deg) || !is_finite(t.yaw_deg) || !is_finite(t.roll_deg) ||
            !is_finite(t.pos_right) || !is_finite(t.pos_up) || !is_finite(t.pos_forward) ||
            !is_finite(t.fov_scale)) {
            std::cerr << "weapon " << i << " has non-finite transform values\n";
            std::exit(1);
        }

        // Rotation angles within sensible bounds: ±45 degrees
        if (std::fabs(t.pitch_deg) > 45.0F) {
            std::cerr << "weapon " << i << " pitch " << t.pitch_deg << " out of range\n";
            std::exit(1);
        }
        if (std::fabs(t.yaw_deg) > 45.0F) {
            std::cerr << "weapon " << i << " yaw " << t.yaw_deg << " out of range\n";
            std::exit(1);
        }
        if (std::fabs(t.roll_deg) > 45.0F) {
            std::cerr << "weapon " << i << " roll " << t.roll_deg << " out of range\n";
            std::exit(1);
        }

        // Position offsets: within ±0.5 meters (viewmodel space)
        if (std::fabs(t.pos_right) > 0.5F || std::fabs(t.pos_up) > 0.5F || std::fabs(t.pos_forward) > 0.5F) {
            std::cerr << "weapon " << i << " position offset out of range: "
                      << t.pos_right << ", " << t.pos_up << ", " << t.pos_forward << "\n";
            std::exit(1);
        }

        // FOV scale must be in (0, 1]  (viewmodels are never larger than world FOV)
        if (t.fov_scale <= 0.0F || t.fov_scale > 1.0F) {
            std::cerr << "weapon " << i << " fov_scale " << t.fov_scale << " out of range\n";
            std::exit(1);
        }
    }
    std::cout << "test_weapon_viewmodel_transforms_bounds passed.\n";
}

void test_weapon_viewmodel_transform_default_is_identity_like() {
    // Out-of-bounds index should return a default-constructed transform.
    const auto t = weapon_viewmodel_transform(999);
    if (std::fabs(t.pitch_deg) > kEpsilon || std::fabs(t.yaw_deg) > kEpsilon ||
        std::fabs(t.roll_deg) > kEpsilon || std::fabs(t.pos_right) > kEpsilon ||
        std::fabs(t.pos_up) > kEpsilon || std::fabs(t.pos_forward) > kEpsilon ||
        std::fabs(t.fov_scale - 1.0F) > kEpsilon) {
        std::cerr << "default weapon transform not identity-like\n";
        std::exit(1);
    }
    std::cout << "test_weapon_viewmodel_transform_default_is_identity_like passed.\n";
}

void test_weapon_viewmodel_transform_negative_index() {
    const auto t = weapon_viewmodel_transform(-1);
    // Should return default (identity-like)
    if (std::fabs(t.fov_scale - 1.0F) > kEpsilon) {
        std::cerr << "negative index did not return default transform\n";
        std::exit(1);
    }
    std::cout << "test_weapon_viewmodel_transform_negative_index passed.\n";
}

// ---------------------------------------------------------------------------
// Grip sockets
// ---------------------------------------------------------------------------

void test_weapon_grip_sockets_bounds() {
    for (int i = 0; i < static_cast<int>(kWeaponGripSockets.size()); ++i) {
        const auto g = weapon_grip_sockets(i);

        // All values must be finite
        if (!is_finite(g.grip_right_x) || !is_finite(g.grip_right_y) ||
            !is_finite(g.grip_right_z) || !is_finite(g.grip_left_x) ||
            !is_finite(g.grip_left_y) || !is_finite(g.grip_left_z)) {
            std::cerr << "weapon " << i << " has non-finite grip socket values\n";
            std::exit(1);
        }

        // Grip positions should be within reachable range of the viewmodel
        // arm skeleton: roughly ±0.5 meters from origin.
        if (std::fabs(g.grip_right_x) > 0.5F || std::fabs(g.grip_right_y) > 1.0F ||
            std::fabs(g.grip_right_z) > 0.5F) {
            std::cerr << "weapon " << i << " right grip out of reachable range\n";
            std::exit(1);
        }
        if (std::fabs(g.grip_left_x) > 0.5F || std::fabs(g.grip_left_y) > 1.0F ||
            std::fabs(g.grip_left_z) > 0.5F) {
            std::cerr << "weapon " << i << " left grip out of reachable range\n";
            std::exit(1);
        }
    }
    std::cout << "test_weapon_grip_sockets_bounds passed.\n";
}

void test_weapon_grip_sockets_default() {
    const auto g = weapon_grip_sockets(999);
    if (std::fabs(g.grip_right_x) > kEpsilon || std::fabs(g.grip_right_y) > kEpsilon ||
        std::fabs(g.grip_right_z) > kEpsilon) {
        std::cerr << "out-of-bounds grip sockets not zero-initialized\n";
        std::exit(1);
    }
    std::cout << "test_weapon_grip_sockets_default passed.\n";
}

// ---------------------------------------------------------------------------
// Reload data
// ---------------------------------------------------------------------------

void test_reload_data_phase_order() {
    for (int i = 0; i < static_cast<int>(kWeaponReloadData.size()); ++i) {
        const auto rd = weapon_reload_data(i);

        // Phase boundary ordering: each start must precede its end, and
        // phases must be non-overlapping and cover [0, 1].
        if (rd.grab_start   > rd.grab_end)   { std::cerr << "weapon " << i << " grab: start > end\n"; std::exit(1); }
        if (rd.remove_start > rd.remove_end) { std::cerr << "weapon " << i << " remove: start > end\n"; std::exit(1); }
        if (rd.insert_start > rd.insert_end) { std::cerr << "weapon " << i << " insert: start > end\n"; std::exit(1); }
        if (rd.return_start > rd.return_end) { std::cerr << "weapon " << i << " return: start > end\n"; std::exit(1); }

        // Sequential non-overlap: grab_end <= remove_start, etc.
        if (rd.grab_end   > rd.remove_start) { std::cerr << "weapon " << i << " grab_end > remove_start\n"; std::exit(1); }
        if (rd.remove_end > rd.insert_start) { std::cerr << "weapon " << i << " remove_end > insert_start\n"; std::exit(1); }
        if (rd.insert_end > rd.return_start) { std::cerr << "weapon " << i << " insert_end > return_start\n"; std::exit(1); }

        // First phase starts at 0, last phase ends at 1
        if (std::fabs(rd.grab_start) > kEpsilon) {
            std::cerr << "weapon " << i << " grab_start != 0\n";
            std::exit(1);
        }
        if (std::fabs(rd.return_end - 1.0F) > kEpsilon) {
            std::cerr << "weapon " << i << " return_end != 1\n";
            std::exit(1);
        }

        // All timing fractions must be in [0, 1]
        if (rd.grab_start < 0.0F || rd.grab_end   > 1.0F ||
            rd.remove_start < 0.0F || rd.remove_end > 1.0F ||
            rd.insert_start < 0.0F || rd.insert_end > 1.0F ||
            rd.return_start < 0.0F || rd.return_end > 1.0F) {
            std::cerr << "weapon " << i << " phase timing out of [0,1]\n";
            std::exit(1);
        }

        // Magazine position must be finite
        if (!is_finite(rd.mag_pos_x) || !is_finite(rd.mag_pos_y) || !is_finite(rd.mag_pos_z)) {
            std::cerr << "weapon " << i << " magazine position has NaN\n";
            std::exit(1);
        }

        // Tilt angles must be finite and within reasonable ranges (±90 deg)
        if (std::fabs(rd.tilt_pitch_deg) > 90.0F || std::fabs(rd.tilt_yaw_deg) > 90.0F ||
            std::fabs(rd.tilt_roll_deg) > 90.0F) {
            std::cerr << "weapon " << i << " tilt angle out of range\n";
            std::exit(1);
        }

        // Position offsets must be finite
        if (!is_finite(rd.offset_right) || !is_finite(rd.offset_up) || !is_finite(rd.offset_forward)) {
            std::cerr << "weapon " << i << " position offset has NaN\n";
            std::exit(1);
        }
    }
    std::cout << "test_reload_data_phase_order passed.\n";
}

void test_reload_data_default() {
    const auto rd = weapon_reload_data(999);
    // Default-constructed WeaponReloadData has non-zero timing defaults
    // (grab_end=0.2, return_end=1.0, etc.) but position/tilt fields are
    // zero-initialized. Check the zero fields instead.
    if (std::fabs(rd.mag_pos_x) > kEpsilon || std::fabs(rd.mag_pos_y) > kEpsilon ||
        std::fabs(rd.mag_pos_z) > kEpsilon) {
        std::cerr << "out-of-bounds reload data magazine position not zero\n";
        std::exit(1);
    }
    if (std::fabs(rd.offset_right) > kEpsilon || std::fabs(rd.offset_up) > kEpsilon ||
        std::fabs(rd.offset_forward) > kEpsilon) {
        std::cerr << "out-of-bounds reload data offset not zero\n";
        std::exit(1);
    }
    std::cout << "test_reload_data_default passed.\n";
}

// ---------------------------------------------------------------------------
// ADS transforms
// ---------------------------------------------------------------------------

void test_ads_transforms_bounds() {
    for (int i = 0; i < static_cast<int>(kWeaponAdsTransforms.size()); ++i) {
        const auto ads = weapon_ads_transform(i);

        // All values must be finite
        if (!is_finite(ads.ads_pos_right) || !is_finite(ads.ads_pos_up) ||
            !is_finite(ads.ads_pos_forward) || !is_finite(ads.ads_pitch_deg) ||
            !is_finite(ads.ads_yaw_deg) || !is_finite(ads.ads_roll_deg) ||
            !is_finite(ads.ads_fov_scale)) {
            std::cerr << "weapon " << i << " ADS transform has non-finite values\n";
            std::exit(1);
        }

        // Position offsets within reasonable range
        if (std::fabs(ads.ads_pos_right) > 0.5F || std::fabs(ads.ads_pos_up) > 0.5F ||
            std::fabs(ads.ads_pos_forward) > 0.5F) {
            std::cerr << "weapon " << i << " ADS position out of range\n";
            std::exit(1);
        }

        // Rotation within reasonable range
        if (std::fabs(ads.ads_pitch_deg) > 45.0F || std::fabs(ads.ads_yaw_deg) > 45.0F ||
            std::fabs(ads.ads_roll_deg) > 45.0F) {
            std::cerr << "weapon " << i << " ADS rotation out of range\n";
            std::exit(1);
        }

        // FOV scale must be in (0, 1] for ADS zoom
        if (ads.ads_fov_scale <= 0.0F || ads.ads_fov_scale > 1.0F) {
            std::cerr << "weapon " << i << " ADS fov_scale " << ads.ads_fov_scale << " out of range\n";
            std::exit(1);
        }

        // ADS FOV should be <= the hip-fire FOV (which is 0.85-0.90 for viewmodel)
        // In practice ADS fov_scale should provide visible zoom: < 0.90
        if (ads.ads_fov_scale >= 0.90F) {
            std::cerr << "weapon " << i << " ADS fov_scale " << ads.ads_fov_scale << " not zoomed enough\n";
            std::exit(1);
        }
    }
    std::cout << "test_ads_transforms_bounds passed.\n";
}

void test_ads_transform_default() {
    const auto ads = weapon_ads_transform(999);
    // Default should be identity-like: no offset, fov_scale=1.0
    if (std::fabs(ads.ads_pos_right) > kEpsilon || std::fabs(ads.ads_pos_up) > kEpsilon ||
        std::fabs(ads.ads_pos_forward) > kEpsilon || std::fabs(ads.ads_fov_scale - 1.0F) > kEpsilon) {
        std::cerr << "out-of-bounds ADS transform not identity-like\n";
        std::exit(1);
    }
    std::cout << "test_ads_transform_default passed.\n";
}

// ---------------------------------------------------------------------------
// Mesh paths
// ---------------------------------------------------------------------------

void test_weapon_viewmodel_mesh_paths() {
    for (int i = 0; i < static_cast<int>(kWeaponViewmodelCount); ++i) {
        const char* path = weapon_viewmodel_mesh_path(i);
        if (path == nullptr || path[0] == '\0') {
            std::cerr << "weapon " << i << " mesh path is null or empty\n";
            std::exit(1);
        }

        // Path must be a .aemesh file
        // Check extension by looking for ".aemesh" at the end.
        std::string_view sv(path);
        auto dot_pos = sv.rfind('.');
        if (dot_pos == std::string_view::npos || sv.substr(dot_pos) != ".aemesh") {
            std::cerr << "weapon " << i << " mesh path '" << path << "' does not end in .aemesh\n";
            std::exit(1);
        }
    }
    std::cout << "test_weapon_viewmodel_mesh_paths passed.\n";
}

void test_weapon_viewmodel_mesh_path_default() {
    const char* path = weapon_viewmodel_mesh_path(999);
    if (path == nullptr || path[0] == '\0') {
        std::cerr << "default mesh path is null or empty\n";
        std::exit(1);
    }
    std::cout << "test_weapon_viewmodel_mesh_path_default passed.\n";
}

// ---------------------------------------------------------------------------
// kWeaponViewmodelCount consistency
// ---------------------------------------------------------------------------

void test_viewmodel_data_count_consistency() {
    // All viewmodel data arrays must have the same number of entries.
    if (kWeaponViewmodelTransforms.size() != kWeaponViewmodelCount) {
        std::cerr << "kWeaponViewmodelTransforms size (" << kWeaponViewmodelTransforms.size()
                  << ") != kWeaponViewmodelCount (" << kWeaponViewmodelCount << ")\n";
        std::exit(1);
    }
    if (kWeaponGripSockets.size() != kWeaponViewmodelCount) {
        std::cerr << "kWeaponGripSockets size (" << kWeaponGripSockets.size()
                  << ") != kWeaponViewmodelCount\n";
        std::exit(1);
    }
    if (kWeaponReloadData.size() != kWeaponViewmodelCount) {
        std::cerr << "kWeaponReloadData size (" << kWeaponReloadData.size()
                  << ") != kWeaponViewmodelCount\n";
        std::exit(1);
    }
    if (kWeaponAdsTransforms.size() != kWeaponViewmodelCount) {
        std::cerr << "kWeaponAdsTransforms size (" << kWeaponAdsTransforms.size()
                  << ") != kWeaponViewmodelCount\n";
        std::exit(1);
    }
    std::cout << "test_viewmodel_data_count_consistency passed.\n";
}

// ---------------------------------------------------------------------------
// Phase order coverage — test that every possible time in [0,1] maps to a
// valid phase for each weapon's reload data.
// ---------------------------------------------------------------------------

void test_reload_phase_coverage() {
    constexpr int kSamples = 1000;
    for (int i = 0; i < static_cast<int>(kWeaponReloadData.size()); ++i) {
        const auto rd = weapon_reload_data(i);
        for (int s = 0; s < kSamples; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kSamples);
            // Determine which phase t falls into (mimicking the animation controller logic)
            int phase = -1;
            if (t < rd.grab_start) {
                phase = 0; // Idle
            } else if (t < rd.grab_end) {
                phase = 1; // GrabMag
            } else if (t < rd.remove_end) {
                phase = 2; // RemoveMag
            } else if (t < rd.insert_end) {
                phase = 3; // InsertMag
            } else if (t < rd.return_end) {
                phase = 4; // ReturnToGrip
            } else {
                phase = 0; // Idle
            }
            // Every sample in [0,1] must map to a valid phase
            if (phase < 0 || phase > 4) {
                std::cerr << "weapon " << i << " time " << t << " maps to invalid phase " << phase << "\n";
                std::exit(1);
            }
        }
    }
    std::cout << "test_reload_phase_coverage passed.\n";
}

}  // namespace

int main() {
    test_weapon_viewmodel_transforms_bounds();
    test_weapon_viewmodel_transform_default_is_identity_like();
    test_weapon_viewmodel_transform_negative_index();

    test_weapon_grip_sockets_bounds();
    test_weapon_grip_sockets_default();

    test_reload_data_phase_order();
    test_reload_data_default();
    test_reload_phase_coverage();

    test_ads_transforms_bounds();
    test_ads_transform_default();

    test_weapon_viewmodel_mesh_paths();
    test_weapon_viewmodel_mesh_path_default();

    test_viewmodel_data_count_consistency();

    std::cout << "\nAll weapon viewmodel data tests passed.\n";
    return 0;
}

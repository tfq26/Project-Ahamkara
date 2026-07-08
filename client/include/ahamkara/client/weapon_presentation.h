#pragma once

#include "ae/render/weapon_model_cache.h"

namespace ae::render {
class RenderBackend;
}

namespace ahamkara::client {

/// Shared first-person weapon presentation cache with animation support.
class WeaponViewmodelPresentation {
public:
    WeaponViewmodelPresentation() = default;

    void set_backend(ae::render::RenderBackend* backend);

    [[nodiscard]] const ae::render::GpuModel* resolve_viewmodel(int weapon_index);

    /// Play a named animation on the current weapon. Returns false if not available.
    bool play_animation(int weapon_index, std::string_view clip_name);

    /// Get joint matrices for the current weapon (for GPU skinning).
    [[nodiscard]] const ae::render::Mat4* joint_matrices(int weapon_index) const;
    [[nodiscard]] int joint_count(int weapon_index) const;

    /// Tick animation drivers.
    void tick(float dt);

    [[nodiscard]] ae::render::WeaponModelCache& cache() { return cache_; }

private:
    ae::render::WeaponModelCache cache_ {};
};

/// Apply two-bone IK to the viewmodel arm skeleton so the hand reaches the
/// weapon grip socket position defined by kWeaponGripSockets.
///
/// @param weapon_index  Current weapon index (resolves grip socket data).
/// @param joint_matrices  Flat array of column-major Mat4 joint matrices
///                        (8 joints * 16 floats each). Modified in-place.
/// @param joint_count  Number of joint matrices in the array.
///
/// The IK operates in model space and adjusts the shoulder (idx 2) and elbow
/// (idx 3) joint rotations so the hand (idx 5) reaches the grip target.
void apply_viewmodel_arm_ik(int weapon_index,
                             float* joint_matrices,
                             int joint_count,
                             const float* ik_offset = nullptr);

}  // namespace ahamkara::client

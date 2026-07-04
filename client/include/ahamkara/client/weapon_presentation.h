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

}  // namespace ahamkara::client

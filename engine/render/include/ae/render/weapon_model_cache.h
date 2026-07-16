#pragma once

#include "ae/animation/clip_player.h"
#include "ae/render/skeletal_animation.h"
#include "ae/render/compiled_mesh.h"
#include "ae/render/render_backend.h"

#include <cstddef>
#include <string>
#include <memory>
#include <unordered_map>

namespace ae::render {

/// Cache for weapon viewmodel meshes and their GPU uploads, plus animation data.
///
/// Load a compiled mesh once, upload it once, and reuse the cached GPU model
/// across weapon swaps.  The compiled mesh also carries animation clips which
/// are played via the internal AnimationClipPlayer.
///
/// ## Runtime Boundary
///
/// This cache is owned by the rendering layer (ae::render).  Gameplay code
/// (ahamkara::game) does NOT access this cache directly.  The gameplay
/// runtime feeds a weapon index to the presentation layer, which resolves
/// the viewmodel path through this cache.  From the gameplay perspective
/// the cache is read-only and opaque.
class WeaponModelCache {
public:
    WeaponModelCache() = default;
    ~WeaponModelCache();

    WeaponModelCache(const WeaponModelCache&) = delete;
    WeaponModelCache& operator=(const WeaponModelCache&) = delete;
    WeaponModelCache(WeaponModelCache&&) = delete;
    WeaponModelCache& operator=(WeaponModelCache&&) = delete;

    void set_backend(RenderBackend* backend);
    void clear();

    /// Get a cached GPU model for a compiled mesh path.
    [[nodiscard]] const GpuModel* get_gpu_model(const std::string& path);

    /// Load animation data for a weapon and get its player.
    /// Returns nullptr if the mesh has no animation data.
    [[nodiscard]] ae::animation::AnimationClipPlayer* get_anim_player(const std::string& path);

    /// Play a named clip on the weapon animations. Returns false if not loaded.
    bool play_animation(const std::string& path, std::string_view clip_name, bool loop = true);

    /// Tick all animation players.
    void tick(float dt);

    /// Get current joint matrices for GPU skinning. Returns nullptr if none.
    [[nodiscard]] const Mat4* current_joint_matrices(const std::string& path) const;
    [[nodiscard]] int current_joint_count(const std::string& path) const;

    [[nodiscard]] std::size_t cached_model_count() const { return entries_.size(); }

private:
    struct Entry {
        GltfModel cpu_model;
        GpuModel gpu_model;
        bool cpu_loaded {false};
        bool gpu_uploaded {false};
        std::unique_ptr<ae::animation::AnimationClipPlayer> anim_player;
    };

    RenderBackend* backend_ {nullptr};
    std::unordered_map<std::string, std::unique_ptr<Entry>> entries_;
};

} // namespace ae::render

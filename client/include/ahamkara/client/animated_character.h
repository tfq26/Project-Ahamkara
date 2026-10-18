#pragma once

#include "ae/animation/clip_player.h"
#include "ae/render/compiled_mesh.h"
#include "ae/render/render_backend.h"

#include <string>

namespace ahamkara::client {

struct CharacterCustomization;

/// Render-owned skinned character used by menu and social presentation screens.
///
/// This intentionally lives outside DebugRenderer: that renderer's humanoid is
/// a diagnostic primitive and its legacy draw path does not preserve skeletal
/// deformation. AnimatedCharacter uses the same PBR/skinning path as authored
/// world assets while CharacterCustomization remains the one appearance source
/// shared by every presentation instance.
class AnimatedCharacter {
public:
    enum class Motion {
        Idle,
        Walk,
        Sprint,
        Dance,
    };

    AnimatedCharacter() = default;
    ~AnimatedCharacter();

    AnimatedCharacter(const AnimatedCharacter&) = delete;
    AnimatedCharacter& operator=(const AnimatedCharacter&) = delete;

    bool initialize(ae::render::RenderBackend& backend, const std::string& compiled_mesh_path,
                    const std::string& profile_mesh_path);
    void destroy();
    void tick(float delta_seconds);
    void set_motion(Motion motion);
    void set_appearance_source(const CharacterCustomization& appearance);

    /// Draw the current pose. All matrices are column-major.
    void render(const float* view_matrix,
                const float* projection_matrix,
                const float* model_matrix);
    /// Render the authored mesh without a skinning palette. Character View
    /// uses this stable neutral presentation while gameplay retains render().
    void render_static(const float* view_matrix,
                       const float* projection_matrix,
                       const float* model_matrix);

    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] int joint_count() const { return animation_.joint_count(); }
    [[nodiscard]] std::string_view current_clip() const { return animation_.current_clip(); }
    [[nodiscard]] Motion motion() const { return motion_; }

private:
    ae::render::RenderBackend* backend_ {nullptr};
    ae::render::GltfModel cpu_model_ {};
    ae::render::GpuModel gpu_model_ {};
    ae::render::GpuModel profile_gpu_model_ {};
    ae::render::ShaderHandle shader_ {};
    ae::animation::AnimationClipPlayer animation_ {};
    const ae::skeleton::Mat4* pose_ {nullptr};
    const CharacterCustomization* appearance_ {nullptr};
    int u_view_ {-1};
    int u_projection_ {-1};
    int u_model_ {-1};
    int u_joints_ {-1};
    int u_use_skinning_ {-1};
    int u_albedo_ {-1};
    Motion motion_ {Motion::Idle};
    bool ready_ {false};

    void render_internal(const float* view_matrix,
                         const float* projection_matrix,
                         const float* model_matrix,
                         bool use_skinning);
};

} // namespace ahamkara::client

#include "ahamkara/client/animated_character.h"
#include "ahamkara/client/client_config.h"

#include "ae/core/log.h"
#include "ae/render/skeletal_animation.h"

#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ahamkara::client {
namespace {

constexpr const char* kCharacterVertexShader = R"(
    #version 330 core
    layout(location = 0) in vec3 aPosition;
    layout(location = 1) in vec4 aJoints;
    layout(location = 2) in vec4 aWeights;
    layout(location = 3) in vec3 aNormal;

    uniform mat4 uView;
    uniform mat4 uProjection;
    uniform mat4 uModel;
    uniform mat4 uJointMatrices[64];
    uniform int uUseSkinning;

    out vec3 vWorldPosition;
    out vec3 vWorldNormal;

    void main() {
        mat4 skin = mat4(1.0);
        if (uUseSkinning != 0) {
            skin = uJointMatrices[int(aJoints.x)] * aWeights.x
                 + uJointMatrices[int(aJoints.y)] * aWeights.y
                 + uJointMatrices[int(aJoints.z)] * aWeights.z
                 + uJointMatrices[int(aJoints.w)] * aWeights.w;
        }
        vec4 localPosition = skin * vec4(aPosition, 1.0);
        vec4 worldPosition = uModel * localPosition;
        vWorldPosition = worldPosition.xyz;
        vWorldNormal = normalize(mat3(uModel) * mat3(skin) * aNormal);
        gl_Position = uProjection * uView * worldPosition;
    }
)";

constexpr const char* kCharacterFragmentShader = R"(
    #version 330 core
    in vec3 vWorldPosition;
    in vec3 vWorldNormal;

    uniform vec3 uAlbedo;
    out vec4 fragColor;

    void main() {
        vec3 normal = normalize(vWorldNormal);
        vec3 sun = normalize(vec3(0.45, 0.82, 0.34));
        vec3 fill = normalize(vec3(-0.65, 0.25, -0.55));
        float keyLight = max(dot(normal, sun), 0.0);
        float fillLight = max(dot(normal, fill), 0.0);
        float skyLight = normal.y * 0.5 + 0.5;
        vec3 light = vec3(0.22, 0.25, 0.31)
                   + vec3(1.0, 0.78, 0.56) * keyLight * 1.15
                   + vec3(0.24, 0.43, 0.62) * fillLight * 0.35
                   + vec3(0.14, 0.19, 0.24) * skyLight;
        vec3 color = uAlbedo * light;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));
        fragColor = vec4(color, 1.0);
    }
)";

ae::skeleton::Skin make_skin(const ae::render::GltfModel& model) {
    return model.skins.empty()
        ? ae::skeleton::Skin {}
        : ae::render::to_skeleton_skin(model.skins.front());
}

std::vector<ae::skeleton::AnimationClipData> make_clips(
    const ae::render::GltfModel& model) {
    std::vector<ae::skeleton::AnimationClipData> clips;
    clips.reserve(model.animations.size());
    for (const auto& animation : model.animations) {
        clips.push_back(ae::render::to_skeleton_clip(animation));
    }
    return clips;
}

bool is_primary_suit_material(const ae::render::GpuMesh& mesh) {
    constexpr float tolerance = 0.025F;
    return std::fabs(mesh.color_r - 0.62F) < tolerance &&
           std::fabs(mesh.color_g - 0.37F) < tolerance &&
           std::fabs(mesh.color_b - 0.17F) < tolerance;
}

} // namespace

AnimatedCharacter::~AnimatedCharacter() {
    destroy();
}

bool AnimatedCharacter::initialize(
    ae::render::RenderBackend& backend,
    const std::string& compiled_mesh_path,
    const std::string& profile_mesh_path) {
    destroy();

    ae::render::CompiledMeshLoader loader;
    if (!loader.load(compiled_mesh_path, cpu_model_)) {
        ae::log_warning_cat(
            "Character",
            "Character asset unavailable: " + compiled_mesh_path + " (" +
                loader.last_error() + ")");
        return false;
    }
    if (cpu_model_.meshes.empty() || cpu_model_.skins.empty() ||
        cpu_model_.animations.empty()) {
        ae::log_warning_cat(
            "Character",
            "Character asset is missing meshes, a skin, or animation clips: " +
                compiled_mesh_path);
        cpu_model_ = {};
        return false;
    }

    if (!animation_.set_data(make_skin(cpu_model_), make_clips(cpu_model_))) {
        ae::log_warning_cat("Character", "Failed to initialize character skeleton.");
        cpu_model_ = {};
        return false;
    }
    // Character View always begins from the authored profile stance. Dance is
    // selected later only when the emote is explicitly requested.
    animation_.play("profile_stance", true);
    if (!animation_.is_playing()) {
        ae::log_warning_cat("Character", "Character profile stance clip is missing.");
        cpu_model_ = {};
        return false;
    }

    backend_ = &backend;
    gpu_model_ = backend.create_gpu_model(cpu_model_);
    if (gpu_model_.meshes.empty()) {
        destroy();
        ae::log_warning_cat("Character", "Character GPU upload produced no meshes.");
        return false;
    }

    ae::render::GltfModel profile_cpu_model;
    ae::render::CompiledMeshLoader profile_loader;
    if (!profile_loader.load(profile_mesh_path, profile_cpu_model) || profile_cpu_model.meshes.empty()) {
        destroy();
        ae::log_warning_cat("Character", "Character profile asset unavailable: " + profile_mesh_path);
        return false;
    }
    profile_gpu_model_ = backend.create_gpu_model(profile_cpu_model);
    if (profile_gpu_model_.meshes.empty()) {
        destroy();
        ae::log_warning_cat("Character", "Character profile GPU upload produced no meshes.");
        return false;
    }

    const int attribute_locations[] = {0, 1, 2, 3};
    const char* attribute_names[] = {"aPosition", "aJoints", "aWeights", "aNormal"};
    const ae::render::ShaderProgramDesc shader_desc {
        kCharacterVertexShader,
        kCharacterFragmentShader,
        attribute_locations,
        attribute_names,
        4,
    };
    shader_ = backend.create_shader_program(shader_desc);
    if (!shader_) {
        destroy();
        ae::log_warning_cat("Character", "Failed to compile the character skinning shader.");
        return false;
    }
    u_view_ = backend.get_uniform_location(shader_, "uView");
    u_projection_ = backend.get_uniform_location(shader_, "uProjection");
    u_model_ = backend.get_uniform_location(shader_, "uModel");
    u_joints_ = backend.get_uniform_location(shader_, "uJointMatrices");
    u_use_skinning_ = backend.get_uniform_location(shader_, "uUseSkinning");
    u_albedo_ = backend.get_uniform_location(shader_, "uAlbedo");

    // The imported human rig's neutral, upright stance is its bind pose. The
    // current profile_stance clip stores identity local transforms and is kept
    // for authoring compatibility, but applying it would erase the MPFB
    // joints' non-identity bind transforms and collapse the skinned mesh.
    pose_ = animation_.rest_pose();
    animation_.pause(true);
    motion_ = Motion::Idle;
    ready_ = pose_ != nullptr && animation_.joint_count() <= 64;
    if (!ready_) {
        destroy();
        ae::log_warning_cat("Character", "Character skeleton exceeds the 64-joint PBR limit.");
        return false;
    }

    ae::log_info_cat(
        "Character",
        "Loaded operative human base: meshes=" + std::to_string(gpu_model_.meshes.size()) +
            " joints=" + std::to_string(animation_.joint_count()) +
            " clips=profile_stance,dance_charleston");
    return true;
}

void AnimatedCharacter::destroy() {
    if (backend_ != nullptr && shader_) {
        backend_->destroy_shader(shader_);
    }
    if (backend_ != nullptr && !gpu_model_.meshes.empty()) {
        backend_->destroy_gpu_model(gpu_model_);
    }
    if (backend_ != nullptr && !profile_gpu_model_.meshes.empty()) {
        backend_->destroy_gpu_model(profile_gpu_model_);
    }
    backend_ = nullptr;
    gpu_model_ = {};
    profile_gpu_model_ = {};
    shader_ = {};
    cpu_model_ = {};
    pose_ = nullptr;
    u_view_ = -1;
    u_projection_ = -1;
    u_model_ = -1;
    u_joints_ = -1;
    u_use_skinning_ = -1;
    u_albedo_ = -1;
    ready_ = false;
    motion_ = Motion::Idle;
}

void AnimatedCharacter::tick(float delta_seconds) {
    if (ready_ && motion_ != Motion::Idle) {
        pose_ = animation_.tick(delta_seconds);
    }
}

void AnimatedCharacter::set_motion(Motion motion) {
    if (!ready_ || motion == motion_) {
        return;
    }
    motion_ = motion;
    switch (motion_) {
        case Motion::Idle:
            animation_.stop();
            pose_ = animation_.rest_pose();
            animation_.pause(true);
            break;
        case Motion::Walk:
            animation_.play("locomotion_walk", true);
            break;
        case Motion::Sprint:
            animation_.play("locomotion_run", true);
            break;
        case Motion::Dance:
            animation_.play("dance_charleston", true);
            break;
    }
    if (motion_ != Motion::Idle) {
        pose_ = animation_.tick(0.0F);
    }
}

void AnimatedCharacter::set_appearance_source(const CharacterCustomization& appearance) {
    appearance_ = &appearance;
}

void AnimatedCharacter::render(
    const float* view_matrix,
    const float* projection_matrix,
    const float* model_matrix) {
    render_internal(view_matrix, projection_matrix, model_matrix, true);
}

void AnimatedCharacter::render_static(
    const float* view_matrix,
    const float* projection_matrix,
    const float* model_matrix) {
    render_internal(view_matrix, projection_matrix, model_matrix, false);
}

void AnimatedCharacter::render_internal(
    const float* view_matrix,
    const float* projection_matrix,
    const float* model_matrix,
    bool use_skinning) {
    if (!ready_ || backend_ == nullptr || (use_skinning && pose_ == nullptr) ||
        view_matrix == nullptr || projection_matrix == nullptr ||
        model_matrix == nullptr) {
        return;
    }

    backend_->use_shader(shader_);
    backend_->set_uniform_mat4_array(u_view_, view_matrix, 1);
    backend_->set_uniform_mat4_array(u_projection_, projection_matrix, 1);
    backend_->set_uniform_mat4_array(u_model_, model_matrix, 1);
    backend_->set_uniform_int(u_use_skinning_, use_skinning ? 1 : 0);
    if (use_skinning) {
        const auto* joint_data = pose_[0].m.data();
        backend_->set_uniform_mat4_array(u_joints_, joint_data, animation_.joint_count());
    }
    const auto& model = use_skinning ? gpu_model_ : profile_gpu_model_;
    for (std::size_t mesh_index = 0; mesh_index < model.meshes.size(); ++mesh_index) {
        const auto& mesh = model.meshes[mesh_index];
        // The static Character View export places the human mesh first and
        // keeps the rifle as separate, authored dark-material meshes.
        if ((!use_skinning && mesh_index == 0) || is_primary_suit_material(mesh)) {
            const auto& appearance = appearance_ != nullptr
                ? *appearance_
                : CharacterCustomization {};
            backend_->set_uniform_vec3(
                u_albedo_, appearance.suit_color_r, appearance.suit_color_g,
                appearance.suit_color_b);
        } else {
            backend_->set_uniform_vec3(u_albedo_, mesh.color_r, mesh.color_g, mesh.color_b);
        }
        // Use the character vertex layout for both variants so the profile
        // mesh supplies its normals to the lit presentation shader.
        backend_->draw_gpu_mesh_skinned(mesh);
    }
    backend_->use_shader(ae::render::kInvalidShader);
}

} // namespace ahamkara::client

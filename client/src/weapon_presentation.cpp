#include "ae/core/log.h"
#include "ahamkara/client/weapon_presentation.h"

#include "ahamkara/client/weapon_viewmodel_data.h"

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {

void WeaponViewmodelPresentation::set_backend(ae::render::RenderBackend* backend) {
    cache_.set_backend(backend);
}

const ae::render::GpuModel* WeaponViewmodelPresentation::resolve_viewmodel(int weapon_index) {
    return cache_.get_gpu_model(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

bool WeaponViewmodelPresentation::play_animation(int weapon_index, std::string_view clip_name) {
    return cache_.play_animation(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index), clip_name, true);
}

const ae::render::Mat4* WeaponViewmodelPresentation::joint_matrices(int weapon_index) const {
    return cache_.current_joint_matrices(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

int WeaponViewmodelPresentation::joint_count(int weapon_index) const {
    return cache_.current_joint_count(ahamkara::client::weapon_viewmodel_mesh_path(weapon_index));
}

void WeaponViewmodelPresentation::tick(float dt) {
    cache_.tick(dt);
}

}  // namespace ahamkara::client


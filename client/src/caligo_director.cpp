#include "ahamkara/client/caligo_director.h"

#include "ae/core/log.h"
#include "ae/render/debug_renderer.h"
#include "ae/render/render_backend.h"

#include <utility>

namespace ahamkara::client {
namespace {

ae::render::LevelAsset make_asset(const char* name, const char* mesh,
                                  float sky_r, float sky_g, float sky_b,
                                  float ambient_r, float ambient_g, float ambient_b) {
    ae::render::LevelAsset asset;
    asset.name = name;
    asset.sky_color_r = sky_r;
    asset.sky_color_g = sky_g;
    asset.sky_color_b = sky_b;
    asset.ambient_r = ambient_r;
    asset.ambient_g = ambient_g;
    asset.ambient_b = ambient_b;

    ae::render::LevelMeshInstance instance;
    instance.mesh_asset_id = mesh;
    asset.mesh_instances.push_back(std::move(instance));
    return asset;
}

}  // namespace

bool CaligoDirector::initialize(ae::render::RenderBackend& backend) {
    exterior_asset_ = make_asset("Caligo Director",
                                 "assets/compiled/models/caligo_director_node.aemesh",
                                 0.12F, 0.045F, 0.018F,
                                 0.12F, 0.065F, 0.035F);
    cutaway_asset_ = make_asset("Caligo Crown Cutaway",
                                "assets/compiled/models/caligo_cutaway.aemesh",
                                0.018F, 0.028F, 0.035F,
                                0.08F, 0.11F, 0.12F);

    exterior_scene_.build(exterior_asset_, backend, "");
    cutaway_scene_.build(cutaway_asset_, backend, "");
    ae::log_info_cat("Client", "Caligo Director geometry: exterior=" +
        std::to_string(exterior_scene_.instance_count()) +
        " cutaway=" + std::to_string(cutaway_scene_.instance_count()));
    ready_ = !exterior_scene_.empty() && !cutaway_scene_.empty();
    if (ready_) {
        ae::log_info_cat("Client", "Caligo Director initialized with exterior and cutaway scenes.");
    } else {
        ae::log_warning_cat("Client", "Caligo Director assets unavailable; Deploy will remain usable without 3D content.");
    }
    return ready_;
}

void CaligoDirector::destroy(ae::render::RenderBackend& backend) {
    exterior_scene_.destroy(backend);
    cutaway_scene_.destroy(backend);
    ready_ = false;
}

const ae::render::LevelRenderScene* CaligoDirector::render_scene() const {
    if (!ready_) return nullptr;
    return view_ == View::Exterior ? &exterior_scene_ : &cutaway_scene_;
}

void CaligoDirector::apply_camera(ae::render::DebugScene& scene) const {
    if (view_ == View::Exterior) {
        scene.camera_position = {0.0F, 0.25F, 5.8F};
        // Aim right of the globe to compose it in the left/middle Director
        // field while keeping the destination detail card unobstructed.
        scene.camera_target = {0.95F, 0.0F, 0.0F};
    } else {
        scene.camera_position = {0.0F, 0.6F, 12.5F};
        scene.camera_target = {2.15F, 0.0F, 0.0F};
    }
    scene.show_crosshair = false;
    scene.show_player_marker = false;
    scene.draw_default_map = false;
    scene.director_space_background = true;
}

}  // namespace ahamkara::client

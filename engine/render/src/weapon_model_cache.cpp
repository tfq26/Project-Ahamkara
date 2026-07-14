#include "ae/render/weapon_model_cache.h"
#include "ae/render/skeletal_animation.h"

#include "ae/core/log.h"

namespace ae::render {

namespace {

ae::skeleton::Skin make_skin(const GltfModel& model) {
    if (model.skins.empty()) {
        return {};
    }
    return to_skeleton_skin(model.skins.front());
}

std::vector<ae::skeleton::AnimationClipData> make_clips(const GltfModel& model) {
    std::vector<ae::skeleton::AnimationClipData> clips;
    clips.reserve(model.animations.size());
    for (const auto& animation : model.animations) {
        clips.push_back(to_skeleton_clip(animation));
    }
    return clips;
}

} // namespace

WeaponModelCache::~WeaponModelCache() {
    clear();
}

void WeaponModelCache::set_backend(RenderBackend* backend) {
    if (backend_ == backend) return;
    clear();
    backend_ = backend;
}

void WeaponModelCache::clear() {
    if (backend_ != nullptr) {
        for (auto& [path, entry] : entries_) {
            if (entry != nullptr && entry->gpu_uploaded) {
                backend_->destroy_gpu_model(entry->gpu_model);
                entry->gpu_model = {};
                entry->gpu_uploaded = false;
            }
        }
    }
    entries_.clear();
}

const GpuModel* WeaponModelCache::get_gpu_model(const std::string& path) {
    auto& entry = entries_[path];
    if (!entry) {
        entry = std::make_unique<Entry>();
    }

    if (!entry->cpu_loaded) {
        CompiledMeshLoader mesh_loader;
        if (!mesh_loader.load(path, entry->cpu_model)) {
            ae::log_warning(std::string("Weapon model cache failed to load '") + path + "': " + mesh_loader.last_error());
            entries_.erase(path);
            return nullptr;
        }
        entry->cpu_loaded = true;

        // Convert render glTF skin/clip data into neutral skeleton data for evaluation.
        if (!entry->cpu_model.animations.empty() && !entry->cpu_model.skins.empty()) {
            entry->anim_player = std::make_unique<ae::animation::AnimationClipPlayer>();
            if (entry->anim_player->set_data(make_skin(entry->cpu_model), make_clips(entry->cpu_model))) {
                entry->anim_player->play("idle", true);
            }
        }
    }

    if (backend_ == nullptr) return nullptr;

    if (!entry->gpu_uploaded) {
        entry->gpu_model = backend_->create_gpu_model(entry->cpu_model);
        entry->gpu_uploaded = true;
    }

    return &entry->gpu_model;
}

ae::animation::AnimationClipPlayer* WeaponModelCache::get_anim_player(const std::string& path) {
    auto it = entries_.find(path);
    if (it == entries_.end() || !it->second) return nullptr;
    // Ensure the entry is loaded
    if (get_gpu_model(path) == nullptr) return nullptr;
    return entries_[path]->anim_player.get();
}

bool WeaponModelCache::play_animation(const std::string& path, std::string_view clip_name, bool loop) {
    auto* player = get_anim_player(path);
    if (!player) return false;
    player->play(clip_name, loop);
    return true;
}

void WeaponModelCache::tick(float dt) {
    for (auto& [path, entry] : entries_) {
        if (entry && entry->anim_player) {
            (void)entry->anim_player->tick(dt);
        }
    }
}

const Mat4* WeaponModelCache::current_joint_matrices(const std::string& path) const {
    auto it = entries_.find(path);
    if (it == entries_.end() || !it->second || !it->second->anim_player) return nullptr;
    return it->second->anim_player->tick(0.0f);  // returns current frame matrices
}

int WeaponModelCache::current_joint_count(const std::string& path) const {
    auto it = entries_.find(path);
    if (it == entries_.end() || !it->second || !it->second->anim_player) return 0;
    return it->second->anim_player->joint_count();
}

} // namespace ae::render

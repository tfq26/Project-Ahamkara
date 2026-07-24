#include "ae/audio/audio_engine.h"
#include "ae/core/log.h"
#include <miniaudio.h>

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <cmath>

#define AE_LOG_CATEGORY "Audio"

namespace ae::audio {

namespace {

/// Map an AudioBus enum to its index in the bus_volumes_ array.
int bus_index(AudioBus bus) {
    return static_cast<int>(bus);
}

/// Compute the effective volume for a sound on a given bus:
/// bus_volume * master_volume * sound_desc_volume.
float compute_volume(float desc_vol, float bus_vol, float master_vol) {
    return desc_vol * bus_vol * master_vol;
}

}  // namespace

// Per-sound spatial tracking for runtime updates.
struct SoundSpatialInfo {
    ma_sound* sound {nullptr};
    AudioBus bus {AudioBus::SFX};
    float base_volume {1.0F};
    float occlusion {0.0F};
    float min_distance {1.0F};
    float max_distance {100.0F};
    float rolloff {1.0F};
};

struct AudioEngine::Impl {
    ma_engine engine;
    bool initialized = false;
    int next_handle = 1;
    std::unordered_map<int, ma_sound*> active_sounds;
    std::unordered_map<int, std::string> loaded_sounds;
    std::unordered_map<int, SoundSpatialInfo> spatial_infos;
    float master_vol = 1.0F;
    RaycastCallback raycast_cb = nullptr;
    // Per-bus volumes, indexed by AudioBus.
    float bus_volumes_[static_cast<int>(AudioBus::Count)] = {
        1.0F,  // Master
        1.0F,  // SFX
        1.0F,  // Weapon
        1.0F,  // Foley
        1.0F,  // Ambience
        1.0F,  // Music
        1.0F,  // UI
    };

    bool init() {
        if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
            log_error_cat(AE_LOG_CATEGORY, "Failed to initialize miniaudio engine");
            return false;
        }
        initialized = true;
        log_info_cat(AE_LOG_CATEGORY, "Audio engine initialized");
        return true;
    }

    void shutdown() {
        for (auto& [h, s] : active_sounds) { ma_sound_uninit(s); delete s; }
        active_sounds.clear();
        spatial_infos.clear();
        if (initialized) { ma_engine_uninit(&engine); initialized = false; }
        log_info_cat(AE_LOG_CATEGORY, "Audio engine shut down");
    }

    int load(const std::string& path) {
        for (auto& [id, p] : loaded_sounds) if (p == path) return id;
        int id = next_handle++;
        loaded_sounds[id] = path;
        log_debug_cat(AE_LOG_CATEGORY, "Loaded sound: " + path);
        return id;
    }

    int play(int id, const AudioDesc& desc) {
        auto it = loaded_sounds.find(id);
        if (it == loaded_sounds.end()) {
            log_warning_cat(AE_LOG_CATEGORY, "play: unknown sound id " + std::to_string(id));
            return -1;
        }

        auto* sound = new ma_sound();
        if (ma_sound_init_from_file(&engine, it->second.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
            log_warning_cat(AE_LOG_CATEGORY, "play: failed to load sound from file: " + it->second);
            delete sound;
            return -1;
        }

        const int bus_idx = bus_index(desc.bus);
        const float effective_vol = compute_volume(desc.volume, bus_volumes_[bus_idx], master_vol);
        ma_sound_set_volume(sound, effective_vol);
        ma_sound_set_position(sound, desc.pos_x, desc.pos_y, desc.pos_z);
        ma_sound_set_looping(sound, desc.loop ? MA_TRUE : MA_FALSE);

        // Enable spatialization if requested.
        if (desc.spatial) {
            ma_sound_set_spatialization_enabled(sound, MA_TRUE);
            ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);
            ma_sound_set_min_distance(sound, desc.min_distance);
            ma_sound_set_max_distance(sound, desc.max_distance);
            ma_sound_set_rolloff(sound, desc.rolloff);
        }

        ma_sound_start(sound);

        int handle = next_handle++;
        active_sounds[handle] = sound;

        // Track spatial info for bus volume / occlusion updates.
        SoundSpatialInfo info;
        info.sound = sound;
        info.bus = desc.bus;
        info.base_volume = desc.volume;
        info.occlusion = 0.0F;
        info.min_distance = desc.min_distance;
        info.max_distance = desc.max_distance;
        info.rolloff = desc.rolloff;
        spatial_infos[handle] = info;

        return handle;
    }

    void stop(int handle) {
        auto it = active_sounds.find(handle);
        if (it != active_sounds.end()) {
            ma_sound_stop(it->second);
            ma_sound_uninit(it->second);
            delete it->second;
            active_sounds.erase(it);
            spatial_infos.erase(handle);
        }
    }

    void set_listener(float x, float y, float z,
                      float fx, float fy, float fz,
                      float ux, float uy, float uz) {
        ma_engine_listener_set_position(&engine, 0, x, y, z);
        ma_engine_listener_set_direction(&engine, 0, fx, fy, fz);
        ma_engine_listener_set_world_up(&engine, 0, ux, uy, uz);
    }

    void set_master(float vol) {
        master_vol = vol;
        ma_engine_set_volume(&engine, vol);
        // Refresh all active sound volumes since master changed.
        for (auto& [handle, sound] : active_sounds) {
            auto sit = spatial_infos.find(handle);
            if (sit != spatial_infos.end()) {
                const int bi = bus_index(sit->second.bus);
                const float occ = 1.0F - sit->second.occlusion;
                ma_sound_set_volume(sound, compute_volume(sit->second.base_volume, bus_volumes_[bi], master_vol) * occ);
            }
        }
    }

    void set_bus_vol(AudioBus bus, float vol) {
        bus_volumes_[bus_index(bus)] = vol;
        // Refresh all active sound volumes on this bus.
        for (auto& [handle, sound] : active_sounds) {
            auto sit = spatial_infos.find(handle);
            if (sit != spatial_infos.end() && sit->second.bus == bus) {
                const float occ = 1.0F - sit->second.occlusion;
                ma_sound_set_volume(sound, compute_volume(sit->second.base_volume, vol, master_vol) * occ);
            }
        }
    }

    float get_bus_vol(AudioBus bus) const {
        return bus_volumes_[bus_index(bus)];
    }

    int play_spatial(int id, const SpatialParams& params, AudioBus bus) {
        auto it = loaded_sounds.find(id);
        if (it == loaded_sounds.end()) {
            log_warning_cat(AE_LOG_CATEGORY, "play_spatial: unknown sound id " + std::to_string(id));
            return -1;
        }

        auto* sound = new ma_sound();
        if (ma_sound_init_from_file(&engine, it->second.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
            log_warning_cat(AE_LOG_CATEGORY, "play_spatial: failed to load sound from file: " + it->second);
            delete sound;
            return -1;
        }

        const int bi = bus_index(bus);
        const float occ = 1.0F - params.occlusion;
        const float effective_vol = compute_volume(1.0F, bus_volumes_[bi], master_vol) * occ;

        ma_sound_set_volume(sound, effective_vol);
        ma_sound_set_position(sound, params.pos_x, params.pos_y, params.pos_z);
        ma_sound_set_velocity(sound, params.vel_x, params.vel_y, params.vel_z);
        ma_sound_set_looping(sound, MA_FALSE);

        // Enable full 3D spatialization.
        ma_sound_set_spatialization_enabled(sound, MA_TRUE);
        ma_sound_set_attenuation_model(sound, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(sound, params.min_distance);
        ma_sound_set_max_distance(sound, params.max_distance);
        ma_sound_set_rolloff(sound, params.rolloff);

        ma_sound_start(sound);

        int handle = next_handle++;
        active_sounds[handle] = sound;

        SoundSpatialInfo info;
        info.sound = sound;
        info.bus = bus;
        info.base_volume = 1.0F;
        info.occlusion = params.occlusion;
        info.min_distance = params.min_distance;
        info.max_distance = params.max_distance;
        info.rolloff = params.rolloff;
        spatial_infos[handle] = info;

        return handle;
    }

    void update_spatial(int handle, const SpatialParams& params) {
        auto it = active_sounds.find(handle);
        if (it == active_sounds.end()) return;

        ma_sound* sound = it->second;
        ma_sound_set_position(sound, params.pos_x, params.pos_y, params.pos_z);
        ma_sound_set_velocity(sound, params.vel_x, params.vel_y, params.vel_z);
        ma_sound_set_min_distance(sound, params.min_distance);
        ma_sound_set_max_distance(sound, params.max_distance);
        ma_sound_set_rolloff(sound, params.rolloff);

        // Update occlusion.
        auto sit = spatial_infos.find(handle);
        if (sit != spatial_infos.end()) {
            sit->second.occlusion = params.occlusion;
            sit->second.min_distance = params.min_distance;
            sit->second.max_distance = params.max_distance;
            sit->second.rolloff = params.rolloff;
            const int bi = bus_index(sit->second.bus);
            const float occ = 1.0F - params.occlusion;
            ma_sound_set_volume(sound, compute_volume(sit->second.base_volume, bus_volumes_[bi], master_vol) * occ);
        }
    }

    void set_occluded(int handle, float occlusion) {
        auto sit = spatial_infos.find(handle);
        if (sit == spatial_infos.end()) return;
        sit->second.occlusion = occlusion;
        auto it = active_sounds.find(handle);
        if (it == active_sounds.end()) return;
        const int bi = bus_index(sit->second.bus);
        const float occ = 1.0F - occlusion;
        ma_sound_set_volume(it->second, compute_volume(sit->second.base_volume, bus_volumes_[bi], master_vol) * occ);
    }
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() = default;
bool AudioEngine::initialize() { return impl_->init(); }
void AudioEngine::shutdown() { impl_->shutdown(); }
int AudioEngine::load_sound(const std::string& p) { return impl_->load(p); }
int AudioEngine::play_sound(int id, const AudioDesc& d) { return impl_->play(id, d); }
void AudioEngine::stop_sound(int h) { impl_->stop(h); }
void AudioEngine::set_listener(float x,float y,float z,float fx,float fy,float fz,float ux,float uy,float uz) {
    impl_->set_listener(x,y,z,fx,fy,fz,ux,uy,uz);
}
void AudioEngine::set_master_volume(float v) { impl_->set_master(v); }
void AudioEngine::set_category_volume(const std::string& category, float vol) {
    // Map string category names to AudioBus enum values.
    if (category == "master")   impl_->set_bus_vol(AudioBus::Master, vol);
    else if (category == "sfx")   impl_->set_bus_vol(AudioBus::SFX, vol);
    else if (category == "weapon") impl_->set_bus_vol(AudioBus::Weapon, vol);
    else if (category == "foley")  impl_->set_bus_vol(AudioBus::Foley, vol);
    else if (category == "ambience") impl_->set_bus_vol(AudioBus::Ambience, vol);
    else if (category == "music")  impl_->set_bus_vol(AudioBus::Music, vol);
    else if (category == "ui")     impl_->set_bus_vol(AudioBus::UI, vol);
    else {
        log_warning_cat(AE_LOG_CATEGORY, "set_category_volume: unknown category '" + category + "'");
    }
}

// --- Bus system ---
void AudioEngine::set_bus_volume(AudioBus bus, float vol) { impl_->set_bus_vol(bus, vol); }
float AudioEngine::get_bus_volume(AudioBus bus) const { return impl_->get_bus_vol(bus); }

// --- 3D spatial audio ---
int AudioEngine::play_spatial(int id, const SpatialParams& params, AudioBus bus) {
    return impl_->play_spatial(id, params, bus);
}
void AudioEngine::update_spatial(int handle, const SpatialParams& params) {
    impl_->update_spatial(handle, params);
}
void AudioEngine::set_occluded(int handle, float occlusion) {
    impl_->set_occluded(handle, occlusion);
}

// --- Occlusion ---
void AudioEngine::set_occlusion_raycast(RaycastCallback cb) {
    impl_->raycast_cb = cb;
}

float AudioEngine::check_occlusion(float ox, float oy, float oz,
                                    float lx, float ly, float lz) {
    if (!impl_->raycast_cb) return 0.0F;

    float dx = lx - ox, dy = ly - oy, dz = lz - oz;
    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.001F) return 0.0F;

    // Normalize direction.
    const float inv = 1.0F / dist;
    dx *= inv; dy *= inv; dz *= inv;

    float hit_dist = 0.0F;
    if (impl_->raycast_cb(ox, oy, oz, dx, dy, dz, dist, hit_dist)) {
        // Occlusion grows as hit moves closer to the source.
        return 1.0F - (hit_dist / dist);
    }
    return 0.0F;
}

// --- Utility ---
float AudioEngine::distance_attenuation(float distance, float min_dist, float max_dist, float rolloff) {
    if (distance <= min_dist) return 1.0F;
    if (distance >= max_dist) return 0.0F;
    // Inverse distance attenuation (matching miniaudio's default model).
    const float clamped = std::max(distance, min_dist);
    return min_dist / (min_dist + rolloff * (clamped - min_dist));
}

} // namespace ae::audio

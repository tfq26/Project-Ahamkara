#include "ae/audio/audio_engine.h"
#include <miniaudio.h>

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <cmath>

namespace ae::audio {

struct AudioEngine::Impl {
    ma_engine engine;
    bool initialized = false;
    int next_handle = 1;
    std::unordered_map<int, ma_sound*> active_sounds;
    std::unordered_map<int, std::string> loaded_sounds;
    float master_vol = 1.0F;

    bool init() {
        if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) return false;
        initialized = true;
        return true;
    }

    void shutdown() {
        for (auto& [h, s] : active_sounds) { ma_sound_uninit(s); delete s; }
        active_sounds.clear();
        if (initialized) { ma_engine_uninit(&engine); initialized = false; }
    }

    int load(const std::string& path) {
        for (auto& [id, p] : loaded_sounds) if (p == path) return id;
        int id = next_handle++;
        loaded_sounds[id] = path;
        return id;
    }

    int play(int id, const AudioDesc& desc) {
        auto it = loaded_sounds.find(id);
        if (it == loaded_sounds.end()) return -1;

        auto* sound = new ma_sound();
        if (ma_sound_init_from_file(&engine, it->second.c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
            delete sound;
            return -1;
        }

        ma_sound_set_volume(sound, desc.volume * master_vol);
        ma_sound_set_position(sound, desc.pos_x, desc.pos_y, desc.pos_z);
        ma_sound_set_looping(sound, desc.loop ? MA_TRUE : MA_FALSE);
        ma_sound_start(sound);

        int handle = next_handle++;
        active_sounds[handle] = sound;
        return handle;
    }

    void stop(int handle) {
        auto it = active_sounds.find(handle);
        if (it != active_sounds.end()) {
            ma_sound_stop(it->second);
            ma_sound_uninit(it->second);
            delete it->second;
            active_sounds.erase(it);
        }
    }

    void set_listener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz) {
        ma_engine_listener_set_position(&engine, 0, x, y, z);
        ma_engine_listener_set_direction(&engine, 0, fx, fy, fz);
        ma_engine_listener_set_world_up(&engine, 0, ux, uy, uz);
    }

    void set_master(float vol) { master_vol = vol; ma_engine_set_volume(&engine, vol); }
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
void AudioEngine::set_category_volume(const std::string&, float) {}

} // namespace ae::audio

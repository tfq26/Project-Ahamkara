#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ae::audio {

/// Named audio buses for independent volume control.
enum class AudioBus : uint8_t {
    Master,
    SFX,
    Weapon,
    Foley,
    Ambience,
    Music,
    UI,
    Count
};

/// Per-bus volume storage (0.0 – 1.0).
struct BusVolumes {
    float master {1.0F};
    float sfx {1.0F};
    float weapon {1.0F};
    float foley {1.0F};
    float ambience {1.0F};
    float music {1.0F};
    float ui {1.0F};
};

/// 3D spatial audio parameters for a sound instance.
struct SpatialParams {
    float pos_x {0.0F}, pos_y {0.0F}, pos_z {0.0F};
    float vel_x {0.0F}, vel_y {0.0F}, vel_z {0.0F};  // for doppler
    float min_distance {1.0F};
    float max_distance {100.0F};
    float rolloff {1.0F};           // 0 = no attenuation, 1 = realistic, >1 = steeper
    float occlusion {0.0F};         // 0 = none, 1 = fully occluded
};

struct AudioDesc {
    float pos_x = 0, pos_y = 0, pos_z = 0;
    float volume = 1.0F;
    bool loop = false;
    AudioBus bus {AudioBus::SFX};   // which bus this sound belongs to
    bool spatial {false};           // enable 3D spatialization
    float min_distance {1.0F};
    float max_distance {100.0F};
    float rolloff {1.0F};
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void shutdown();

    int load_sound(const std::string& path);
    int play_sound(int id, const AudioDesc& desc = {});
    void stop_sound(int handle);
    void set_listener(float x, float y, float z,
                      float fx, float fy, float fz,
                      float ux, float uy, float uz);
    void set_master_volume(float vol);
    void set_category_volume(const std::string& category, float vol);

    // --- Bus system ---
    void set_bus_volume(AudioBus bus, float vol);
    [[nodiscard]] float get_bus_volume(AudioBus bus) const;

    // --- 3D spatial audio ---
    /// Play a sound with full 3D spatial parameters.
    int play_spatial(int id, const SpatialParams& params, AudioBus bus = AudioBus::SFX);
    /// Update spatial properties of a playing sound handle.
    void update_spatial(int handle, const SpatialParams& params);
    /// Mark a playing sound as occluded (sets occlusion factor, applied as volume reduction).
    void set_occluded(int handle, float occlusion);

    // --- Utility ---
    static float distance_attenuation(float distance, float min_dist, float max_dist, float rolloff);
    static float check_occlusion(float ox, float oy, float oz,
                                 float lx, float ly, float lz);  // stub

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::audio

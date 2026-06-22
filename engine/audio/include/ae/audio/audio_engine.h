#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ae::audio {

struct AudioDesc {
    float pos_x = 0, pos_y = 0, pos_z = 0;
    float volume = 1.0F;
    bool loop = false;
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
    void set_listener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz);
    void set_master_volume(float vol);
    void set_category_volume(const std::string& category, float vol);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ae::audio

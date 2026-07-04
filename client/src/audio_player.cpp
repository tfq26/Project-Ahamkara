#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "ahamkara/client/audio_player.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

#define AE_LOG_CATEGORY "Audio"

namespace ahamkara::client {
namespace {

// --- Procedural sound generation -----------------------------------------------

struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t overall_size;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt_chunk_marker[4] = {'f', 'm', 't', ' '};
    uint32_t length_of_fmt = 16;
    uint16_t format_type = 1; // PCM
    uint16_t channels = 1;
    uint32_t sample_rate = 44100;
    uint32_t byterate = 44100 * 2;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    char data_chunk_header[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};

void write_wav_file(const std::string& path, const std::vector<int16_t>& samples) {
    WavHeader header;
    header.data_size = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    header.overall_size = header.data_size + sizeof(WavHeader) - 8;

    std::ofstream out(path, std::ios::binary);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(header.data_size));
}

std::vector<int16_t> generate_hit_sound() {
    constexpr int sample_rate = 44100;
    constexpr float duration = 0.05F;
    constexpr int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<int16_t> samples(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        float envelope = std::exp(-50.0F * t);
        float angle = 2.0F * 3.14159265F * 1500.0F * t;
        samples[i] = static_cast<int16_t>(32767.0F * envelope * std::sin(angle));
    }
    return samples;
}

std::vector<int16_t> generate_crit_sound() {
    constexpr int sample_rate = 44100;
    constexpr float duration = 0.12F;
    constexpr int num_samples = static_cast<int>(sample_rate * duration);
    std::vector<int16_t> samples(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        float envelope = std::exp(-25.0F * t);
        float angle1 = 2.0F * 3.14159265F * 2200.0F * t;
        float angle2 = 2.0F * 3.14159265F * 1100.0F * t;
        samples[i] = static_cast<int16_t>(16383.0F * envelope * (std::sin(angle1) + std::sin(angle2)));
    }
    return samples;
}

std::string get_sound_path(const std::string& filename) {
    if (fs::exists("assets")) {
        return "assets/" + filename;
    }
    fs::path p = ".";
    for (int i = 0; i < 4; ++i) {
        p = p / "..";
        if (fs::exists(p / "assets")) {
            return (p / "assets" / filename).lexically_normal().string();
        }
    }
    return filename;
}

void ensure_sound_files() {
    std::string hit_path = get_sound_path("hit.wav");
    std::string crit_path = get_sound_path("crit.wav");

    if (!fs::exists(hit_path)) {
        write_wav_file(hit_path, generate_hit_sound());
    }
    if (!fs::exists(crit_path)) {
        write_wav_file(crit_path, generate_crit_sound());
    }
}

// Map sound keys to WAV file paths (extensible for asset streaming).
std::string resolve_sound_path(const std::string& sound_key) {
    if (sound_key == "hit")  return get_sound_path("hit.wav");
    if (sound_key == "crit") return get_sound_path("crit.wav");
    // Future: add keys for footsteps, weapon fire, etc.
    // For now, return empty — unknown sounds are silently skipped.
    return {};
}

} // namespace

// --- AudioPlayerImpl -----------------------------------------------------------

class AudioPlayerImpl {
public:
    ma_engine engine;
    bool initialized {false};

    /// Per-category volume multipliers (0.0 = mute, 1.0 = nominal).
    float category_volumes_[7] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    /// Master enable/disable.
    bool audio_enabled {true};

    AudioPlayerImpl() {
        ensure_sound_files();
        ma_result result = ma_engine_init(nullptr, &engine);
        if (result == MA_SUCCESS) {
            initialized = true;
        } else {
            ae::log_error_cat("Audio", "miniaudio: Failed to initialize ma_engine. Error: " + std::to_string(result));
        }
    }

    ~AudioPlayerImpl() {
        if (initialized) {
            ma_engine_uninit(&engine);
        }
    }

    void set_category_volume(ahamkara::game::AudioCategory cat, float vol) {
        auto idx = static_cast<int>(cat);
        if (idx >= 0 && idx < 7) {
            category_volumes_[idx] = std::clamp(vol, 0.0f, 1.0f);
        }
    }

    float get_category_volume(ahamkara::game::AudioCategory cat) const {
        auto idx = static_cast<int>(cat);
        if (idx >= 0 && idx < 7) {
            return category_volumes_[idx];
        }
        return 1.0f;
    }

    /// Compute the effective volume for an event after applying category and
    /// event-specific multipliers.
    [[nodiscard]] float effective_volume(const ahamkara::game::AudioEvent& event) const {
        float cat_vol = get_category_volume(event.category);
        return std::clamp(event.volume * cat_vol, 0.0f, 1.0f);
    }

    void play_event_internal(const ahamkara::game::AudioEvent& event) {
        if (!initialized || !audio_enabled) return;

        float vol = effective_volume(event);
        if (vol <= 0.0f) return;

        std::string path = resolve_sound_path(event.sound_key ? event.sound_key : "");
        if (path.empty()) return;

        ma_engine_set_volume(&engine, vol);
        ma_engine_play_sound(&engine, path.c_str(), nullptr);
    }
};

// --- AudioPlayer ---------------------------------------------------------------

AudioPlayer::AudioPlayer()
    : impl_(std::make_unique<AudioPlayerImpl>()) {
}

AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::play_sound(const std::string& name) {
    // Convert legacy call to an event and forward.
    ahamkara::game::AudioEvent evt;
    evt.sound_key = name.c_str();
    evt.volume = 1.0f;
    evt.category = ahamkara::game::AudioCategory::SFX;
    play_event(evt);
}

void AudioPlayer::play_event(const ahamkara::game::AudioEvent& event) {
    if (!impl_) return;
    impl_->play_event_internal(event);
}

void AudioPlayer::apply_config(const AudioConfig& audio_cfg) {
    if (!impl_) return;
    impl_->audio_enabled = audio_cfg.enabled;
    impl_->set_category_volume(ahamkara::game::AudioCategory::Master,  audio_cfg.master_volume);
    impl_->set_category_volume(ahamkara::game::AudioCategory::SFX,     audio_cfg.sfx_volume);
    impl_->set_category_volume(ahamkara::game::AudioCategory::Weapon,  audio_cfg.weapon_volume);
    impl_->set_category_volume(ahamkara::game::AudioCategory::UI,      audio_cfg.ui_volume);
    impl_->set_category_volume(ahamkara::game::AudioCategory::Music,   audio_cfg.music_volume);
    impl_->set_category_volume(ahamkara::game::AudioCategory::Ambient, audio_cfg.ambient_volume);
}

void AudioPlayer::set_category_volume(ahamkara::game::AudioCategory category, float volume) {
    if (impl_) impl_->set_category_volume(category, volume);
}

float AudioPlayer::get_category_volume(ahamkara::game::AudioCategory category) const {
    return impl_ ? impl_->get_category_volume(category) : 1.0f;
}

}  // namespace ahamkara::client

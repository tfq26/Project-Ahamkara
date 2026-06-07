#pragma once

#include "ahamkara/client/client_config.h"
#include "ahamkara/game/audio_events.h"
#include "ahamkara/game/world.h"
#include <memory>
#include <string>

namespace ahamkara::client {

class AudioPlayerImpl;

/**
 * @brief Concrete implementation of the IAudioPlayer interface using miniaudio.
 *
 * Supports:
 *  - Event-based audio dispatch via play_event()
 *  - Per-category volume control
 *  - Backward-compatible play_sound()
 *  - Client-config-driven audio settings
 */
class AudioPlayer : public ahamkara::game::IAudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer() override;

    /// Backward-compatible convenience: play a named sound at full volume.
    void play_sound(const std::string& name) override;

    /// Play a structured audio event with category/volume/position metadata.
    void play_event(const ahamkara::game::AudioEvent& event) override;

    /// Apply per-category volume settings from client config.
    void apply_config(const AudioConfig& audio_cfg);

    /// Set category volume at runtime (0.0 = mute, 1.0 = nominal).
    void set_category_volume(ahamkara::game::AudioCategory category, float volume);

    /// Get current volume for a category.
    [[nodiscard]] float get_category_volume(ahamkara::game::AudioCategory category) const;

private:
    std::unique_ptr<AudioPlayerImpl> impl_;
};

}  // namespace ahamkara::client

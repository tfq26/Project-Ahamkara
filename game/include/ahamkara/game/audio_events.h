#pragma once

#include <cstdint>

namespace ahamkara::game {

enum class AudioCategory : std::uint8_t {
    Master = 0,
    SFX,
    Weapon,
    UI,
    Music,
    Ambient
};

struct AudioEvent {
    const char* sound_key {nullptr};
    float volume {1.0f};
    AudioCategory category {AudioCategory::SFX};
};

}  // namespace ahamkara::game

#pragma once

/// SAFE for headless/server — only constructs AudioEvent structs, never calls
/// IAudioPlayer. All functions are pure factory helpers.

#include "ahamkara/game/audio_events.h"
#include "ahamkara/game/movement.h"

#include <algorithm> // for std::min

namespace ahamkara::game::adapters {

/// Construct a weapon-fire audio event.
[[nodiscard]] inline AudioEvent make_weapon_fire_event()
{
    return AudioEvent{"weapon_fire", 1.0f, AudioCategory::Weapon};
}

/// Construct a weapon-reload audio event.
[[nodiscard]] inline AudioEvent make_weapon_reload_event()
{
    return AudioEvent{"weapon_reload", 0.7f, AudioCategory::Weapon};
}

/// Construct a player-jump audio event.
[[nodiscard]] inline AudioEvent make_jump_event()
{
    return AudioEvent{"player_jump", 0.5f, AudioCategory::SFX};
}

/// Construct a player-landing audio event.
/// Volume scales with impact speed, clamped to [0, 1].
[[nodiscard]] inline AudioEvent make_landing_event(float impact_speed)
{
    return AudioEvent{"player_land",
                      std::min(1.0f, 0.3f + impact_speed * 0.1f),
                      AudioCategory::SFX};
}

/// Construct a footstep audio event for the given surface material.
/// Currently a placeholder — could be extended to map each SurfaceMaterial
/// to a distinct sound key (e.g. "footstep_concrete", "footstep_grass").
[[nodiscard]] inline AudioEvent make_footstep_event(SurfaceMaterial /*mat*/)
{
    return AudioEvent{"footstep_default", 0.4f, AudioCategory::SFX};
}

/// Construct a generic hit-received audio event.
[[nodiscard]] inline AudioEvent make_hit_event()
{
    return AudioEvent{"dummy_hit", 0.8f, AudioCategory::SFX};
}

/// Construct a bullet-impact audio event (world-space impact).
[[nodiscard]] inline AudioEvent make_bullet_impact_event()
{
    return AudioEvent{"bullet_impact", 0.6f, AudioCategory::SFX};
}

/// Construct a UI audio event keyed by an arbitrary string.
/// Typical use: button clicks, menu open/close, notifications.
[[nodiscard]] inline AudioEvent make_ui_event(const char* key)
{
    return AudioEvent{key, 0.6f, AudioCategory::UI};
}

}  // namespace ahamkara::game::adapters

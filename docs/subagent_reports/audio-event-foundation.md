# Task
Add event-based audio dispatch and config-driven audio categories as foundational improvements for multiplayer FPS audio architecture.

# Outcome
Fully implemented: event-based audio queuing (`queue_audio_event` / `flush_audio_events`), `AudioEvent` struct with position/category/volume/surface metadata, `AudioCategory` enum (7 buses), `SurfaceType` enum, per-category volume control on `AudioPlayer`, `AudioConfig` struct parsed from `ahamkara.cfg`, backward-compatible `play_sound()`.

Partially implemented: the docs (`audio_architecture.md`) describe a complete plan for all 10 focus areas (3D spatial, occlusion, reverb zones, voice chat, streaming, etc.) but only the event dispatch + config backbone is in code.

Not implemented: 3D spatialization via `ma_sound_set_position`, actual `ma_sound_group` sub-mix buses, surface-dependent footstep sample resolution, occlusion raycasts, reverb zone ECS triggers, networked sound event prediction, audio asset streaming, debug audio visualizer, voice chat integration.

# Files Changed
- `game/include/ahamkara/game/audio_events.h` — **New.** `AudioEvent` struct, `AudioCategory` enum, `SurfaceType` enum
- `game/include/ahamkara/game/world.h` — Added `play_event()` to `IAudioPlayer`; added `queue_audio_event()`, `flush_audio_events()`, event queue array (`kMaxAudioEventsPerTick = 64`)
- `game/src/world.cpp` — Implemented queue/flush; replaced direct `audio_player_->play_sound()` call in projectile hit with `queue_audio_event(AudioEvent{...})`; added `flush_audio_events()` at end of `tick_internal()`
- `client/include/ahamkara/client/audio_player.h` — Added `play_event()`, `apply_config()`, `set_category_volume()`, `get_category_volume()`
- `client/src/audio_player.cpp` — Category volume array (7 floats), `effective_volume()` with clamp, `resolve_sound_path()` lookup table, config application; added `#include <algorithm>` for `std::clamp`, `#include "ae/core/log.h"` for categorized logging; removed unused `<unordered_map>`
- `client/include/ahamkara/client/client_config.h` — Added `AudioConfig` struct (enabled + 6 volume fields)
- `client/src/client_config.cpp` — Parses 7 new config keys: `audio_enabled`, `audio_master_volume`, `audio_sfx_volume`, `audio_weapon_volume`, `audio_ui_volume`, `audio_music_volume`, `audio_ambient_volume`
- `client/config/ahamkara.cfg` — Added commented audio section with all 7 keys
- `client/src/debug_client.cpp` — Added `audio_player.apply_config(client_config.audio)` after construction
- `docs/audio_architecture.md` — **New.** Full audio architecture document covering all 10 focus areas (81–90)
- `docs/architecture.md` — Added companion documents section; added "Partial Implementation" note for audio events under Event Bus section
- `docs/client_config.md` — Added audio config keys table + updated example config

# Interfaces Added Or Changed
- **New public struct:** `ahamkara::game::AudioEvent` — carries `sound_key` (const char*), `position` (Vec3), `volume` (float), `category` (AudioCategory), `surface` (SurfaceType), `is_2d` (bool)
- **New enum:** `ahamkara::game::AudioCategory` — Master, SFX, Weapon, UI, Voice, Music, Ambient
- **New enum:** `ahamkara::game::SurfaceType` — Default, Concrete, Metal, Wood, Dirt, Grass, Water, Glass
- **Changed class:** `ahamkara::game::IAudioPlayer` — added `virtual void play_event(const AudioEvent& event) = 0`
- **New public methods on World:** `queue_audio_event(const AudioEvent&)`, `flush_audio_events()`, `kMaxAudioEventsPerTick` constant
- **New public methods on AudioPlayer:** `play_event()`, `apply_config()`, `set_category_volume()`, `get_category_volume()`
- **New config struct:** `ahamkara::client::AudioConfig` — `enabled`, `master_volume`, `sfx_volume`, `weapon_volume`, `ui_volume`, `music_volume`, `ambient_volume`
- **New config keys:** `audio_enabled`, `audio_master_volume`, `audio_sfx_volume`, `audio_weapon_volume`, `audio_ui_volume`, `audio_music_volume`, `audio_ambient_volume`

# Behavior
- `debug_client.cpp` (local play mode) now reads audio config from `ahamkara.cfg` and applies per-category volumes to `AudioPlayer` at startup.
- Projectile hits now queue `AudioEvent` structs with position and category metadata instead of calling `audio_player_->play_sound()` directly. Events are flushed at the end of `tick_internal()`.
- `AudioPlayer::play_sound()` still works — it wraps the name in an `AudioEvent` and delegates.
- Server/headless/sandbox modes: `World` has no `IAudioPlayer` set, so `flush_audio_events()` is a no-op. Events are silently discarded.
- Setting `audio_enabled = false` in config disables all sound output (checked before each event).
- Setting any category volume to 0.0 mutes that bus.

# Validation
- Build command: `cmake --build build --target ahamkara_client -j8`
- Object files compiled successfully:
  - `game/CMakeFiles/ahamkara_game.dir/src/world.cpp.o` — ✅
  - `client/CMakeFiles/ahamkara_client.dir/src/audio_player.cpp.o` — ✅
  - `client/CMakeFiles/ahamkara_client.dir/src/client_config.cpp.o` — ✅
  - `game/CMakeFiles/ahamkara_game.dir/src/client_prediction.cpp.o` (server path) — ✅
- Full `ahamkara_client` target failed due to pre-existing error in `game/src/gameplay_types.cpp` (unknown type `HealthComponent` on line 310 of `gameplay_types.h`). This error is unrelated to these changes and was documented in `architecture.md` Next Recommended Tasks as a pre-existing issue.
- EntT deprecation warnings (`operator"" _hs`, `operator"" _hws`) are pre-existing, not introduced by this work.
- No tests were run (the smoke tests target may also be affected by the pre-existing build break).

# Known Gaps
- `AudioEvent::position` is carried through the pipeline but not consumed — 3D spatialization (`ma_sound_set_position`) is not wired yet.
- `AudioEvent::surface` is carried but `resolve_sound_path()` ignores it — surface-dependent footstep resolution is not implemented.
- `AudioEvent::is_2d` flag is defined but unused.
- Per-category volume is applied per-event via `ma_engine_set_volume()` (global engine volume) rather than per-sound-group. This means playing two sounds at once from different categories may produce incorrect individual volumes. Proper `ma_sound_group` bus routing is documented in `audio_architecture.md` but not implemented.
- Event queue is a fixed-size array (64/tick) with silent overflow. No diagnostic warning when the queue is full.
- The `Voice` category and `Music`/`Ambient` categories have config keys and volume sliders but no sounds are routed to them yet.
- `resolve_sound_path()` has only two entries (`"hit"`, `"crit"`). Unknown keys return empty string and are silently skipped with no warning log.
- The flashback sample, sandbox client, and windowed client have no `AudioPlayer` at all (correct — they shouldn't by design), but they also don't read `AudioConfig` (they just use the default `ClientConfig.audio` which has all volumes at 1.0). This is intentional: `AudioConfig` is only applied in `debug_client.cpp` because it's the only path that creates an `AudioPlayer`.

# Risks
- The `gameplay_types.cpp` compilation error blocks the full `ahamkara_client` target from building. Merging this branch will not make things worse, but a full build will still fail until that pre-existing issue is fixed.
- The `ma_engine_set_volume()` call in `play_event_internal()` sets the **global** engine volume before each sound. Since miniaudio's `ma_engine_play_sound()` returns immediately (fire-and-forget), there is a race: if two events from different categories are flushed in the same tick, the second event's `ma_engine_set_volume()` will override the first before the first sound finishes playing. Currently only one event fires per tick (projectile hit), so this is theoretical. Fix: use `ma_sound_group` per category.
- `AudioConfig` struct adds 7 fields to `ClientConfig`. If `ClientConfig` becomes large enough to affect stack usage at construction, this could matter, but currently the struct is tiny.

# Next Recommended Steps
1. Fix the pre-existing `gameplay_types.cpp` build error so the full target compiles end-to-end.
2. Wire `ma_sound_group` per `AudioCategory` so per-bus volume mixing is correct when multiple sounds play simultaneously. This is the core of #84 (Audio Mixer with Buses).
3. Wire `ma_sound_set_position()` in `play_event_internal()` using `AudioEvent::position` from the event queue to enable basic 3D panning (#81).
4. Add surface-dependent sound resolution to `resolve_sound_path()` — append surface name to base key (e.g. `"footstep_Metal"` → `"assets/audio/footsteps/metal/footstep_01.ogg"`) — for #83.
5. Add weapon fire audio event queuing in `spawn_projectile()` using `AudioCategory::Weapon` — this is a one-line addition and is the first step for #82 (low-latency weapon path).
6. Add a `queue_full_warning` counter or log when the 64-event-per-tick queue overflows.
7. Implement the debug audio visualizer (#89) by exposing `AudioPlayer::active_sound_count()` and drawing spheres in `DebugRenderer`.

# Notes For Integration
- **No CMake changes needed.** All new headers are in existing include directories. No new source files were added to the build. No new dependencies.
- **Server remains audio-free.** The server's `CMakeLists.txt` does not link miniaudio. `IAudioPlayer` is an abstract interface in the `game` layer; only the client provides a concrete implementation.
- The `game/include/ahamkara/game/audio_events.h` header transitively depends on `game/include/ahamkara/game/net_types.h` (for `Vec3`). This is intentional — `Vec3` is a game-layer type. The dependency graph is: `audio_events.h` → `net_types.h` → `ae/core/types.h`.
- `AudioPlayer` now depends on `AudioConfig` (from `client_config.h`), which means `audio_player.h` includes `client_config.h`. This is the first coupling between audio and config at the header level; previously they were separate compilation units.
- The existing behavior of `debug_client.cpp` is preserved — hit/crit sounds still play. The only runtime change is that audio config is now applied at startup (with defaults of 1.0/true, so no audible change unless the user edits the config file).

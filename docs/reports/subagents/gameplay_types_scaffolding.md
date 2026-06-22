# Task
Implement core multiplayer FPS gameplay data types and systems (areas 91-100), prioritizing server-authoritative scaffolding and testable pure-data structs.

# Outcome

## Fully Implemented
- **Teams (95):** `Team` enum (None/Spectator/Red/Blue/FFA), `can_damage()`, `team_color()`, `TeamColor`
- **Game Modes (95):** `GameModeType` enum (5 modes), `GameModeRules` with score/time limits, round count, respawn config, auto-start, min players
- **Spawn System (96):** `SpawnPoint` (per-team, priority, enabled), `SpawnSelector` with avoid-recently-used logic
- **Damage Model (93):** `DamageEvent` struct, `ArmorConfig` (CS-style: 66% absorption, 2:1 durability ratio, helmet headshot bypass), `apply_damage()` pure function
- **Health/Armor/Status Effects (94):** `StatusEffectType` enum (8 types), `StatusEffectInstance` (tick-based duration/magnitude), `ArmorConfig`, `KillFeedEntry`
- **Weapon Framework (91):** `WeaponDefinition` (all params: fire modes, spread/recoil, falloff, projectile physics, melee, charge, ADS), `WeaponState` with `can_fire()`/`can_reload()`, `FireMode` enum, `RecoilEntry`
- **Inventory/Loadout (97):** `WeaponSlot` enum, `Loadout`, `PlayerLoadoutSelection`
- **Match State Machine (98):** `MatchPhase` enum (Lobby→Warmup→Countdown→InProgress→Overtime→RoundEnd→MatchEnd→PostMatch), `MatchState::tick()` with score/time/overtime/round logic, full lifecycle
- **Replay (100):** `ReplayFrameHeader` (per-tick envelope)

## Partially Implemented
- **Projectile/Hitscan (92):** Structs defined but no per-tick fire dispatch. Collision engine already supports ray/sphere/capsule traces, `HitboxInstance`, and `CollisionWorld::query_aabb()`.

## Not Implemented
- Per-tick weapon firing (spread cone, recoil, hitscan dispatch)
- Server-side lag-compensated hitscan (rewind-and-replay against `ServerHistoryBuffer`)
- Bot/player simulation harness (99)
- Full replay file format and playback (100)
- Integration of new types into existing `World` class, `ServerSnapshot`, or network packets

# Files Changed
- `game/include/ahamkara/game/gameplay_types.h` — new: 457-line header with all gameplay data structs, enums, and `apply_damage()` inline function
- `game/src/gameplay_types.cpp` — new: `SpawnSelector::select()`, `MatchState::tick()`, `MatchState::set_phase()`, `MatchState::add_score()` implementations (208 lines)
- `tests/src/gameplay_tests.cpp` — new: 28 unit tests covering all 10 focus areas (606 lines)
- `game/CMakeLists.txt` — added `src/gameplay_types.cpp` to `ahamkara_game` library
- `tests/CMakeLists.txt` — added `ahamkara_gameplay_tests` executable and CTest registration
- `game/include/ahamkara/game/components.h` — added missing `#include "ahamkara/game/net_types.h"` and qualified `ae::u32`/`ae::u8`; fixes latent compilation bug exposed when `gameplay_types.h` transitively includes `components.h`

# Interfaces Added Or Changed

## New Structs/Enums
| Name | Namespace | Header |
|------|-----------|--------|
| `Team` | `ahamkara::game` | `gameplay_types.h` |
| `TeamColor` | `ahamkara::game` | `gameplay_types.h` |
| `GameModeType` | `ahamkara::game` | `gameplay_types.h` |
| `GameModeRules` | `ahamkara::game` | `gameplay_types.h` |
| `SpawnPoint` | `ahamkara::game` | `gameplay_types.h` |
| `SpawnSelector` | `ahamkara::game` | `gameplay_types.h` |
| `WeaponSlot` | `ahamkara::game` | `gameplay_types.h` |
| `FireMode` | `ahamkara::game` | `gameplay_types.h` |
| `RecoilEntry` | `ahamkara::game` | `gameplay_types.h` |
| `WeaponDefinition` | `ahamkara::game` | `gameplay_types.h` |
| `WeaponState` | `ahamkara::game` | `gameplay_types.h` |
| `Loadout` | `ahamkara::game` | `gameplay_types.h` |
| `PlayerLoadoutSelection` | `ahamkara::game` | `gameplay_types.h` |
| `ArmorConfig` | `ahamkara::game` | `gameplay_types.h` |
| `DamageEvent` | `ahamkara::game` | `gameplay_types.h` |
| `StatusEffectType` | `ahamkara::game` | `gameplay_types.h` |
| `StatusEffectInstance` | `ahamkara::game` | `gameplay_types.h` |
| `KillFeedEntry` | `ahamkara::game` | `gameplay_types.h` |
| `MatchPhase` | `ahamkara::game` | `gameplay_types.h` |
| `MatchState` | `ahamkara::game` | `gameplay_types.h` |
| `ReplayFrameHeader` | `ahamkara::game` | `gameplay_types.h` |

## New Functions
| Signature | File | Type |
|-----------|------|------|
| `TeamColor team_color(Team)` | `gameplay_types.h` | inline |
| `bool can_damage(Team, Team, Team)` | `gameplay_types.h` | inline |
| `bool apply_damage(HealthComponent&, float, bool, const ArmorConfig&, DamageEvent&)` | `gameplay_types.h` | inline |
| `bool SpawnSelector::select(...)` | `gameplay_types.cpp` | out-of-line |
| `bool MatchState::tick(float, const GameModeRules&)` | `gameplay_types.cpp` | out-of-line |
| `void MatchState::set_phase(MatchPhase, float)` | `gameplay_types.cpp` | out-of-line |
| `void MatchState::add_score(Team, u32, u16)` | `gameplay_types.cpp` | out-of-line |

## Changed Existing Contracts
- `components.h`: now includes `net_types.h` (no include-cycle risk; `net_types.h` includes only `ae/core/types.h`)
- `components.h`: `ProjectileComponent::owner_id` changed from `u32` to `ae::u32` (was previously compiling due to transitive includes from `world.h`)
- `components.h`: `MovementComponent::State` changed from `u8` to `ae::u8` (same reason)

## New Tests
- `ahamkara_gameplay_tests` — 28 assertions in `tests/src/gameplay_tests.cpp`; registered with CTest

# Behavior
- All new types are pure-data or pure-logic structs with **zero dependency on renderer, audio, platform, or physics backends**. They can be instantiated and tested in a headless unit-test harness.
- `apply_damage()` computes armor absorption, headshot bypass, and lethal detection entirely from its parameters — no global state.
- `MatchState::tick()` drives the full Lobby→PostMatch lifecycle, including overtime tie-breaking for team modes and multi-round transitions.
- `SpawnSelector::select()` picks spawns by team, priority, and least-recently-used heuristic.

# Validation
- **Build:** `cmake -B build -DCMAKE_BUILD_TYPE=Debug` re-ran successfully; `ahamkara_game` static library and `ahamkara_gameplay_tests` executable compiled without errors.
- **Tests (CTest):** `ctest --output-on-failure` — **6/6 suites passed, 0 failures**:
  - `ahamkara_smoke_tests` — passed
  - `ahamkara_world_tests` — passed
  - `ahamkara_movement_tests` — passed
  - `ahamkara_collision_tests` — passed
  - `ahamkara_gameplay_tests` — passed (new, 28 tests)
  - `ahamkara_asset_pipeline_tests` — passed
- **Warnings:** None from new code. Pre-existing CMake deprecation warnings from `glm-src` (unrelated).

# Known Gaps
- `WeaponDefinition` has a `std::vector<RecoilEntry>` member, making it non-trivially-copyable. This is fine for server-side config tables but will need a flat array or separate serialization path before sending over the network.
- `MatchState::individual_score` is a fixed array of 12 slots. Should be resizable or keyed by `player_id` once dynamic player joining is implemented.
- `SpawnSelector::m_last_used_tick` has a hard `kMaxLastUsed=256` cap; silently ignores indices beyond this.
- `WeaponState::can_reload()` was originally broken (compared `ammo_in_magazine < definition_index`). Fixed by adding `magazine_capacity` field.
- `apply_damage()` headshot bypass was originally buggy (added extra damage even when shield=0). Fixed by adding `hc.shield > 0.0F` guard.
- No serialization functions for new packet types — these structs have **not** been added to `net_packets.h`.

# Risks
- **Latent bug fixed in `components.h`:** The missing `#include "ahamkara/game/net_types.h"` and unqualified `u32`/`u8` worked before only because `world.h` (which includes both `net_types.h` and `components.h`) was always the first inclusion. Adding a direct `#include "components.h"` from `gameplay_types.h` exposed this. The fix is backward-compatible.
- **Include order sensitivity:** `gameplay_types.h` must include `components.h` before using `HealthComponent`. If `components.h` doesn't have `net_types.h` included first, it will fail. This is now fixed.
- **No runtime integration:** These types are not wired into `World`, `ServerSnapshot`, or the network packet serializer yet. Adding them will require new packet types and corresponding `serialize`/`deserialize` functions in `net_packets.h`.

# Next Recommended Steps
1. **Add network packet serialization** for `DamageEvent`, `KillFeedEntry`, `MatchState`, and `ServerSnapshot` extensions (health/armor fields). Create `PacketType::DamageEvent` and `PacketType::MatchState`.
2. **Wire `MatchState` into the server:** The dedicated server (`server/src/dedicated_server_main.cpp`) should own a `MatchState` and `GameModeRules`, tick them each server step, and broadcast phase transitions.
3. **Extend `ServerSnapshot`** with `health`, `shield`, `team` fields so clients can render team-colored nameplates and HUD health bars.
4. **Integrate `apply_damage()` into `World`:** Replace hardcoded `damage=25.0F` in `world.cpp`'s projectile hit handling with `apply_damage()` calls that produce `DamageEvent`s.
5. **Add `WeaponDefinition` table:** Create a static `std::vector<WeaponDefinition>` of default weapons (pistol, rifle, shotgun, sniper, rocket) and reference them by index from `WeaponState::definition_index`.
6. **Implement per-tick weapon firing** on the server: spread cone calculation, recoil pattern application, hitscan ray dispatch via `CollisionWorld`, ammo consumption, cooldown — all behind the prediction/lag-comp boundary.
7. **Add `StatusEffectInstance` to ECS** as a component so entities can have multiple active effects that tick each server step.

# Notes For Integration
- The new header is entirely self-contained gameplay logic. It can be included from any server or test TU without pulling in Jolt, SDL, miniaudio, or EnTT.
- When creating `PacketType::MatchState` etc., be aware that `MatchState` has a `ae::u16 individual_score[12]` field. This is a fixed-size array and **is** trivially-copyable, so it can use the existing `ByteWriter::write()`/`ByteReader::read()` approach without changes.
- `WeaponDefinition` cannot be serialized with the current `std::memcpy` approach because it contains `std::vector<RecoilEntry>`. Either strip `recoil_pattern` from the network-facing struct, or serialize it separately as a count-prefixed sequence.
- The `components.h` fix is minimal and safe — it only adds an include and qualifies two type names. No existing behavior changes.

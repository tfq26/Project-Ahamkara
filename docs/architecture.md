# Ahamkara Engine Foundation

## Status: First Slice Implemented

The engine foundation provides the systems needed for deterministic FPS simulation
with separate render/simulation ticks, hot-reloadable configuration, and a clear
path toward multi-threaded job execution.

## Companion Documents

- **[Audio Architecture](audio_architecture.md)** — Event-based audio dispatch,
  mixer bus design, per-feature audio plans (3D spatial, occlusion, reverb,
  voice chat, etc.)
- **[Client Configuration](client_config.md)** — Config file format and keys
- **[Asset Pipeline](asset_pipeline.md)** — Asset baking and streaming
- **[Networking](networking.md)** — Server-authoritative netcode
- **[Building](building.md)** — Build instructions

---

## 1. Fixed Timestep Simulation (`ae/core/tick.h`)

### Implemented

`ae::FixedTimestepAccumulator` decouples simulation rate from render rate.

```
   Render thread                    Simulation (game thread)
   ─────────────                    ────────────────────────
   dt = measure_frame_time()
   acc.accumulate(dt)
   while (acc.consume()):           world.tick(acc.step())
       world.tick(acc.step())       for each fixed step
   alpha = acc.interpolation_alpha()
   render(alpha)                    // smooth visual interpolation
```

- **Default tick rate:** 60 Hz (configurable via `ConfigVar`)
- **Spiral-of-death guard:** Max 8 simulation steps per frame; excess is dropped
- **Interpolation alpha:** Value in [0,1] passed to renderer for smooth visual state
- **Reset:** `reset()` clears accumulator after large pauses or world loads

### Integration point

The `LocalPlaySimulation::tick()` in `game/src/world.cpp` currently takes a raw
`delta_seconds`. The migration path:

1. Wrap the simulation loop with `FixedTimestepAccumulator`
2. Change `world.tick(dt, input)` to `world.tick(fixed_step, input)`
3. Feed `interpolation_alpha()` to the renderer for interpolated positions

---

## 2. Separate Render Tick from Simulation Tick

### Design (not yet implemented in client loop)

The current client loop (`client/src/debug_client.cpp`) is a single-threaded
`while` loop. The planned architecture:

```
┌─────────────────────────────────────────────────────┐
│ FRAME                                               │
│                                                     │
│  ┌──────────┐   ┌──────────┐   ┌───────────────┐   │
│  │ Input    │ → │ Sim Tick │ → │ Render Tick   │   │
│  │ Gather   │   │ (fixed   │   │ (variable     │   │
│  │          │   │  dt)     │   │  framerate)   │   │
│  └──────────┘   └──────────┘   └───────────────┘   │
│                                                     │
│  Phase 1: Single-threaded (current)                 │
│    input → tick → tick → tick → render              │
│                                                     │
│  Phase 2: Threaded (future)                         │
│    Thread A: input → tick → tick → tick             │
│    Thread B: render(interpolated_state)             │
│    Shared: double-buffered game state snapshot      │
└─────────────────────────────────────────────────────┘
```

### Implementation steps

1. Add `FixedTimestepAccumulator` to `LocalPlaySimulation` (trivial)
2. Move render state preparation to use `interpolation_alpha()`
3. (Future) Spin off render thread with shared game state ring buffer

---

## 3. Deterministic Game-State Update Rules

### Implemented

`ae::DeterministicRng` — Xorshift64 PRNG with explicit seed control.

### Rules for determinism

For client-side prediction and server reconciliation to work correctly,
all game-state mutations must be deterministic:

1. **Fixed timestep only.** No variable-delta logic in game state.
2. **Deterministic RNG for gameplay.** Use `ae::DeterministicRng` for
   spread patterns, damage randomization, AI decisions. Never use
   `std::rand()`, `std::mt19937`, or time-based seeds for gameplay.
3. **No floating-point branching.** `if (a < b)` where a,b are floats
   must produce identical results on all platforms. Use `std::isfinite`
   checks and explicit epsilon comparisons where needed.
4. **Input is the sole non-deterministic input.** The server seeds its
   RNG once. Clients predict by replaying inputs with a synced seed.
5. **No unordered containers for game state.** `std::unordered_map`
   iteration order varies by platform. Use `std::map`, sorted vectors,
   or fixed-size arrays for simulation state.
6. **Visual-only code is exempt.** Particles, decals, screen shake,
   and rendering effects can use non-deterministic RNG freely.

---

## 4. Engine-Wide Job System (Design)

### Requirements

- Lightweight: no external dependencies beyond C++20
- Work-stealing thread pool
- Job graph with dependencies (fork-join)
- Main thread can submit and wait
- Suitable for: physics broadphase, animation evaluation, particle update,
  network packet serialization

### Design sketch

```
ae::JobSystem (engine/core/include/ae/core/job_system.h)
├── JobSystem::init(thread_count = hardware_concurrency - 1)
├── JobSystem::submit(JobFunction, dependencies...)
├── JobSystem::wait(job_handle)  // block until complete
├── JobSystem::wait_all()        // drain all pending
└── JobSystem::shutdown()
```

### Job types

```
struct JobDeclaration {
    void (*execute)(void* data);   // function pointer
    void* data;                     // opaque payload
    JobHandle parent;               // optional dependency
};
```

### Integration points

- **Physics:** JoltPhysics `PhysicsSystem::Update()` can run individual
  body pair collision checks as jobs
- **Animation:** `evaluate_animation()` per-character could be job-ified
- **Networking:** Serialize/deserialize snapshots in parallel
- **Particles:** Update batches in parallel

### Deferred

Full implementation is deferred until the simulation tick is decoupled
from the render thread. The current single-threaded loop doesn't need
a job system yet.

---

## 5. Frame Allocator / Temp Memory Arena (Design)

### Requirements

- Bump allocator: O(1) allocation, O(1) reset
- Per-frame lifetime: reset at end of each frame
- Suitable for: temporary vectors in tick(), render command buffers,
  network packet assembly
- Fallback to heap on overflow (or assert in debug)

### Design sketch

```
ae::FrameAllocator (engine/core/include/ae/core/frame_allocator.h)
├── FrameAllocator(size_t arena_size = 4 MB)
├── void* allocate(size_t size, size_t alignment = 16)
├── void reset()          // rewind to start (called each frame)
├── size_t used() const   // high-water mark for tuning
└── size_t capacity() const
```

### Usage pattern

```cpp
ae::FrameAllocator g_frame_alloc;

void game_loop() {
    g_frame_alloc.reset();  // start of frame

    // Any temporary allocations this frame:
    std::vector<int, ae::FrameAllocatorAdapter<int>> temp_vec(&g_frame_alloc);
    temp_vec.push_back(42);

    // ... render, tick, etc ...

    // Allocator is reset at next frame start
}
```

### Deferred

Implement after the job system, since jobs need their own scratch arenas.
A single-threaded bump allocator is trivial; the complexity is in making
it work with multiple job threads (per-thread arenas or lock-free).

---

## 6. Entity/Component Storage (Design)

### Current state

EnTT (v3.13.0) is already a dependency. The `ahamkara::game::World` class
has a private `entt::registry registry_` member — but it's unused.
Game state is currently stored as plain structs (`ReplicatedPlayerState`,
`ProjectileState`, `TargetDummyState`) in fixed-size arrays within `World`.

### Planned architecture

```
ahamkara::game::World
├── entt::registry registry_          // ECS storage
│   ├── entity: Player
│   │   ├── TransformComponent
│   │   ├── CharacterMovementComponent
│   │   └── HealthComponent
│   ├── entity: Projectile (x64)
│   │   ├── TransformComponent
│   │   ├── ProjectileComponent
│   │   └── LifetimeComponent
│   └── entity: TargetDummy (x4)
│       ├── TransformComponent
│       └── HealthComponent
└── FixedTimestepAccumulator acc_
```

### Component types (planned)

```cpp
struct TransformComponent {
    Vec3 position;
    Vec3 velocity;
    float yaw;
};

struct HealthComponent {
    float current;
    float max;
};

struct ProjectileComponent {
    u32 owner_id;
    float damage;
    bool is_hitscan;
};

struct LifetimeComponent {
    float remaining_seconds;
};
```

### Migration strategy

1. Define component structs in `game/include/ahamkara/game/components/`
2. Add systems (free functions) that iterate entities with specific components
3. Gradually replace `World` member arrays with registry queries
4. Keep the existing public API (`get_player_state()`, etc.) as convenience
   accessors backed by ECS queries

### Deferred

Full ECS migration should happen after the fixed-timestep refactor.
The current struct-based approach is simple and correct for the small
entity counts (1 player, 4 dummies, 64 projectiles).

---

## 7. Engine Event / Message Bus (Design)

### Requirements

- Type-safe event dispatch
- Immediate (synchronous) and queued (deferred) delivery
- Subscriber registration by event type
- Suitable for: damage events, UI notifications, audio triggers,
  debug command routing

### Design sketch

```
ae::EventBus (engine/core/include/ae/core/event_bus.h)
├── EventBus::subscribe<EventType>(callback) → SubscriptionHandle
├── EventBus::emit(event)              // immediate delivery
├── EventBus::enqueue(event)           // deferred (processed next poll)
├── EventBus::poll()                   // drain queued events
└── SubscriptionHandle::unsubscribe()
```

### Event types (planned)

```cpp
struct DamageEvent {
    u32 target_id;
    u32 source_id;
    float amount;
    Vec3 hit_position;
    bool is_critical;
};

struct AudioTriggerEvent {
    std::string sound_name;
    Vec3 position;
    float volume;
};

struct ConfigChangedEvent {
    std::string key;
    // value accessible via ConfigVar::get()
};
```

### Integration

- `ConfigVar::on_change()` already fires local callbacks. Could be extended
  to emit `ConfigChangedEvent` on the bus.
- `DamageEvent` replaces direct calls to `spawn_damage_number()` in World.
  Audio and UI subsystems subscribe independently.
- `AudioTriggerEvent` decouples gameplay from `IAudioPlayer`.

### Partial Implementation

- **Audio events** (`game/include/ahamkara/game/audio_events.h`) are live.
  The `World` uses an internal event queue (`queue_audio_event` / `flush_audio_events`)
  rather than the full `EventBus`. This avoids the overhead of a general-purpose
  message bus for the single subscriber (AudioPlayer) and keeps the audio path
  lean. See [Audio Architecture](audio_architecture.md) for details.

### Deferred

Implement after ECS migration. The existing direct function calls are
sufficient for the current entity count.

---

## 8. Hot-Reloadable Config Variables (`ae/core/config.h`)

### Implemented

`ae::ConfigVar<T>` and `ae::ConfigRegistry` provide:

- **Typed variables:** `ConfigVar<float>`, `ConfigVar<int>`, `ConfigVar<bool>`,
  `ConfigVar<std::string>`. Extendable via template specialization.
- **Change callbacks:** `on_change([](T old, T val) { ... })` fires on
  programmatic set() or file reload.
- **File format:** `key=value` per line, `#` comments, whitespace-trimmed.
- **Hot reload:** `poll_reload(path)` checks file modification time.
  Call once per frame (or every N frames).
- **Save:** `save_to_file(path)` writes all registered vars sorted by key.

### Usage

```cpp
// Declare (e.g. in game_module.cpp)
ae::ConfigVar<float> g_player_speed("game.player_speed", 5.5F);
ae::ConfigVar<int>   g_max_projectiles("game.max_projectiles", 64);
ae::ConfigVar<bool>  g_debug_physics("debug.show_physics", false);

// Register for hot-reload
g_player_speed.on_change([](float old, float val) {
    ae::log_info_cat("Config", "player_speed changed: " +
                     std::to_string(old) + " -> " + std::to_string(val));
});

// Per-frame (in main loop)
ae::ConfigRegistry::instance().poll_reload("ahamkara_live.cfg");
```

### Auto-registration

`ConfigVar` constructor calls `ConfigRegistry::instance().register_var()`.
No manual registration needed. The `parse_value`/`serialize_value` functions
provide the round-trip to/from the file format.

---

## 9. Runtime Console Command System (Design)

### Requirements

- Register commands by name with typed arguments
- In-game console overlay (~ key)
- Command history (up/down arrow)
- Auto-complete
- Suitable for: `say`, `kick`, `teleport`, `give_ammo`, `toggle_fps`,
  `set_config key value`, `reload_config`

### Design sketch

```
ae::Console (engine/core/include/ae/core/console.h)
├── Console::register_command("name", "help text", callback)
├── Console::execute("name arg1 arg2") → std::string result
├── Console::auto_complete("prefix") → std::vector<std::string>
├── Console::history_up() / history_down()
└── Console::render(float x, float y, float width, float height)
```

### Command model

```cpp
// Built-in commands
CONSOLE_COMMAND(reload_config, "Reload hot-reloadable config file") {
    int n = ae::ConfigRegistry::instance().poll_reload("ahamkara_live.cfg");
    return "Reloaded " + std::to_string(n) + " variables.";
}

CONSOLE_COMMAND(set, "Set a config variable: set <key> <value>") {
    // Look up config var by key and set it
}
```

### Integration

- The console renderer is a debug overlay drawn after the HUD.
- Input capture: when console is open, keyboard input routes to console
  instead of game. Tilde/backtick toggles.
- Commands can be called from code via `Console::execute()` — this lets
  the event bus or config system trigger commands.

### Deferred

Full console implementation is lower priority than simulation/render
decoupling and the job system. A minimal `Console::execute()` with
no UI can be added first.

---

## 10. Structured Logging Categories (`ae/core/log.h`)

### Implemented

Added to existing `log_info`/`log_warning`/`log_error`:

```cpp
void log_info_cat(std::string_view category, std::string_view message);
void log_warning_cat(std::string_view category, std::string_view message);
void log_error_cat(std::string_view category, std::string_view message);
```

### Output format

```
[Info][Render][12.345] DebugRenderer initialized
[Warning][Network][12.456] Packet loss detected
[Error][Config][12.567] Could not open config file
```

### Usage convention

Define a file-local category macro:

```cpp
#define AE_LOG_CATEGORY "Render"
// ... in functions:
log_info_cat(AE_LOG_CATEGORY, "VBO upload complete");
```

### Planned expansions

- **Log levels (compile-time):** `AE_LOG_LEVEL` define to strip verbose
  categories from release builds
- **File/line annotation:** In debug builds, prepend `[file.cpp:123]`
- **Ring buffer:** Keep last N log lines in memory for crash dump
- **Remote log sink:** Send logs to a connected debugger or network client

---

## Files Changed in This Slice

| File | Change |
|------|--------|
| `engine/core/include/ae/core/log.h` | Added `log_info_cat`, `log_warning_cat`, `log_error_cat` with category parameter |
| `engine/core/src/log.cpp` | Implemented categorized logging with `[Category]` tag in output |
| `engine/core/include/ae/core/tick.h` | **New.** `DeterministicRng` (Xorshift64) + `FixedTimestepAccumulator` |
| `engine/core/include/ae/core/config.h` | **New.** `ConfigVar<T>` template + `ConfigRegistry` singleton |
| `engine/core/src/config.cpp` | **New.** Config file parser, hot-reload poller, save/load |
| `engine/core/CMakeLists.txt` | Added `src/config.cpp` |
| `docs/architecture.md` | **Replaced.** Full engine foundation architecture document |

## Tests

- `ahamkara_smoke_tests` — Passed
- `ahamkara_world_tests` — Passed
- `ahamkara_server` — Pre-existing compilation error (unrelated to this slice)
- `ahamkara_client` — Pre-existing compilation error (unrelated to this slice)

## Next Recommended Tasks

1. **Fix pre-existing build errors** in `client/src/headless_clients.cpp` and
   `server/src/dedicated_server_main.cpp` so the full project builds.

2. **Integrate `FixedTimestepAccumulator`** into `LocalPlaySimulation` —
   replace the raw `delta_seconds` pass-through with fixed-step accumulation.

3. **Wire `interpolation_alpha()`** into the render pipeline so dummies and
   projectiles use interpolated positions.

4. **Register game config vars** (`player_speed`, `max_projectiles`, etc.)
   with `ConfigVar` and add a `reload_config` console command stub.

5. **Implement `ae::JobSystem`** once simulation is on a fixed timestep —
   start with parallel particle updates as a proof of concept.

6. **Implement `ae::FrameAllocator`** — the bump allocator is trivial and
   useful immediately for temporary vectors in the render path.

7. **ECS migration** — define component types, start using EnTT queries
   for projectile and dummy management.

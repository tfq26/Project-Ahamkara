# 09 — Game Design & Fun Factor

Audit date: 2026-10-16 (America/Chicago)
Repository state: dirty working tree observed by Recon at `b705d74`; conclusions below describe the inspected working tree, not clean HEAD.
Overall rating: **2/5 — rudimentary gameplay building blocks, no verified complete fun game**

## 1. Scope & Method

Inspected Flashback launch behavior, activity catalog, level specs, weapon/perk definitions, player inventory/progression API, encounter definitions, horde/deathmatch activity implementations, combat feedback, and asset pipeline docs. Evidence comes from `samples/flashback/src/main.cpp`, `assets/activities/activities.json`, `assets/levels/*`, `game/`, `tools/blender/weapons/`, and `docs/systems/asset_pipeline.md`. No code or assets were changed. No GUI was launched: Recon's isolated headless build failed to link a dirty-tree UI test, and Recon explicitly did not attempt graphical runtime launch. Consequently, visual quality, responsiveness, controls, pacing, accessibility, perceived balance, and actual fun are **UNVERIFIED**; this report judges the design affordances and source-level behavior only. No user-specified target genre or definition of “fun” is present in the owner context.

## 2. Executive Summary

- The demonstrable product direction is a 3D first-person shooter/action game: the activity catalog names PvP Clash, PvE horde defense, a social hub, and PvEvP Gambit, and the game code has weapons, movement, combat, enemies, and levels (`assets/activities/activities.json:1-34`; `game/src/world.cpp`; `game/src/activities/`).
- **There is not enough evidence to call any included demo fun or even verify a playable end-to-end loop.** Flashback defaults to menu/Director and explicitly does not load gameplay without a level or autoplay option (`samples/flashback/src/main.cpp:8-13,27-29,33-76`). GUI playtesting was unavailable.
- There are useful moment-to-moment building blocks: three registered weapons, recoil, damage feedback, damage numbers, audio event hooks, abilities, and a kill-all dummy condition (`game/include/ahamkara/game/weapon_registry.h:10-60`; `game/src/world.cpp:920-949,966-989`).
- Encounter schemas cover kill/survive/defend/collect/reach/boss objectives and timer/player-enter/enemy-death triggers (`game/include/ahamkara/game/encounter_scripting.h:21-105`). They establish breadth, but do not constitute a designer-facing authoring/balance workflow by themselves.
- Progression/inventory/currency are implemented as player-side data APIs; the reviewed evidence does not establish a complete reward loop, persistence/reward presentation, or integration with the demo (`game/include/ahamkara/game/inventory.h:32-71`; `game/include/ahamkara/game/player.h:222-244`).
- Level design is text-authored and can describe spawn points, collision, and mesh references; the provided Javelin-4 map is a large collision-oriented arena. Authoring is repeatable but primitive and not visibly playtest/balance guided (`docs/systems/asset_pipeline.md:61-79,113-120`; `assets/levels/javelin4.lvl:1-79`).
- The standalone Horde activity has a thin wave loop and simple objective-seeking enemies, but its source does not show enemy-death cleanup, kill/score credit, between-wave rewards, difficulty choices, or a complete player combat integration (`game/src/activities/horde_activity.cpp:49-60,148-189`).
- Priority should be a single polished reference game with repeatable playtest and balance evidence, rather than adding more genre claims, catalogs, or progression fields.

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Genre/product identity | `assets/activities/activities.json`, `samples/flashback/` | PARTIAL | 2 | FPS/action direction is apparent; listed modes exceed verified experiences. |
| Flashback playable demo | `samples/flashback/src/main.cpp`, `samples/flashback/game_module/` | PARTIAL | 2 | Boots client/module; default launches menu/Director rather than gameplay. GUI playtest unavailable. |
| Weapons and combat feel | `game/include/ahamkara/game/weapon_registry.h`, `game/src/world.cpp`, `game/src/world_projectile.cpp` | PARTIAL | 3 | Combat feedback exists; authored balance and play feel unverified. |
| Abilities | `game/include/ahamkara/game/player.h`, `game/src/` | PARTIAL | 2 | Grenade/special/ultimate state APIs exist; full tactical design/effect loop not assessed as complete. |
| Objective and encounter design | `game/include/ahamkara/game/encounter_scripting.h`, `game/src/encounter_scripting.cpp` | PARTIAL | 3 | Broad data model for objectives, triggers, and waves; authoring and runtime presentation need validation. |
| PvE Horde | `game/src/activities/horde_activity.cpp`, `game/include/ahamkara/game/activities/horde_activity.h` | PARTIAL | 2 | Timed waves and basic seek AI; objective/wave loop is not demonstrably integrated into a playable experience. |
| PvP Deathmatch | `game/src/activities/deathmatch_activity.cpp`, `game/src/gameplay_types.cpp` | PARTIAL | 2 | Match rules and level load exist; no actual multiplayer match was run. |
| Progression, inventory, currency | `game/include/ahamkara/game/inventory.h`, `game/src/pickup_model.cpp`, `game/include/ahamkara/game/player.h` | PARTIAL | 2 | Data structures and grant logic exist; full reward UX, persistence, and pacing are unverified. |
| Level authoring | `assets/levels/`, `tools/levelgen/`, `docs/systems/asset_pipeline.md` | PARTIAL | 2 | Text specs and generated levels; mostly low-level geometry/collision authoring. |
| Weapon archetype/perk catalog | `tools/blender/weapons/archetypes.json`, `tools/blender/weapons/perks.json` | PARTIAL | 2 | Large authored stat/perk vocabulary exists; runtime mapping and balance validation not proven. |
| Balance/playtest tools | `tests/`, `tools/` | MISSING | 1 | No inspected evidence of bot playtesting, tuning reports, telemetry-to-balance loop, or scenario balance harness. |

## 4. Detailed Findings

### GDES-001 — No verified complete reference game or fun-factor evidence

- **Severity:** BLOCKER
- **Category:** missing feature
- **Evidence:** Flashback's usage notes state that without `--level` it opens at menu/Director and does not load a gameplay level; the default level is null (`samples/flashback/src/main.cpp:8-13,27-29`). The executable passes a gameplay level only with explicit `--level`, or uses a generated autoplay scenario when `--autoplay` is given (`samples/flashback/src/main.cpp:33-76`). Recon reports no GUI demo launch and a failed dirty-tree headless build at the UI test link step (`audit/01-recon.md`, Scope & Method and validation steps 3/6).
- **Impact:** The mission is a game engine for AI agents to create complete, polished, fun games. Today there is no verified golden game whose controls, objectives, pacing, game feel, and completion loop demonstrate that capability. A code inventory cannot substitute for a played vertical slice.
- **Recommended fix:** Select one small target experience (recommend a short single-player arena survival/shooter given current FPS assets and systems). Make the default sample enter a playable level; define a 5–10 minute loop with clear goal, threat escalation, reward, failure, and restart. Add an automated scenario that boots, plays a scripted/bot path, asserts win/loss and captures a screenshot/state trace; require human playtest notes for feel claims.
- **Effort:** L
- **Dependencies:** QA and AI verification reports; owner genre/platform decision.

### GDES-002 — Horde mode's pressure loop lacks demonstrated counterplay and round closure

- **Severity:** HIGH
- **Category:** design flaw
- **Evidence:** Horde starts a new wave when a timer expires and accelerates the interval down to 10 seconds (`game/src/activities/horde_activity.cpp:49-60`). It fills a fixed 16-enemy array, caps each wave at 16, and increases health based on wave number (`game/src/activities/horde_activity.cpp:148-169`). Enemies move directly toward the origin and damage the objective only when within 0.1 world units (`game/src/activities/horde_activity.cpp:171-189`). The header snapshot has wave, timer, objective health, and team score (`game/include/ahamkara/game/activities/horde_activity.h:26-37`), but this implementation does not award team score or remove/resolve killed enemies in the inspected file.
- **Impact:** The nominal escalating defense mode lacks visible authored choices, spawn variety beyond type IDs, a documented rest/reward phase, or a strong round-end/continue decision. Timer-driven overlapping waves can fill the fixed array and replace existing entries; gameplay quality and exact runtime effect are unverified, but the implementation provides little foundation for carefully tuned escalating challenge.
- **Recommended fix:** Define explicit wave state transitions (intermission, spawn, active, clear, complete/fail), bounded spawn scheduling that waits on alive counts, per-archetype threat/health/speed budgets, objective attacks/telegraphs, player counterplay and reward choices. Add deterministic wave tests and a balancing scenario that records time-to-kill, objective damage, and outcomes across player skill/loadout profiles.
- **Effort:** M
- **Dependencies:** GDES-001; gameplay systems audit.

### GDES-003 — Progression and economy are data primitives without a demonstrated reward loop

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** `ProgressionState` tracks level, XP, and an XP threshold; `CurrencyState` tracks Glimmer and provides spend/earn methods (`game/include/ahamkara/game/inventory.h:32-71`). Player exposes inventory, progression, and currency (`game/include/ahamkara/game/player.h:222-244`). Horde snapshots contain `team_score`, but its tick path has no score updates (`game/src/activities/horde_activity.cpp:49-60,192-216`).
- **Impact:** A level/XP field and wallet do not create meaningful progression. No inspected evidence connects completed objectives or kills to authored rewards, unlocks, pacing, persistence, clear choice, or feedback. This limits roguelite/looter/campaign loops and makes the catalog of perks difficult to evaluate as player-facing progression.
- **Recommended fix:** Choose whether progression is session-only or persistent. For the first vertical slice, define one earn/spend loop (e.g. wave reward choice among three weapon upgrades), deterministic reward tables, caps and anti-softlock rules, visible explanation, and save/reload tests. Keep long-term meta progression out until this loop is fun and measured.
- **Effort:** M
- **Dependencies:** GDES-001; asset/save-load and AI ergonomics audits.

### GDES-004 — Level authoring is geometry-first, with weak support for encounter pacing and balance

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** The asset pipeline documents `.lvl` for world settings, spawn points, collision boxes, and mesh references, and gives a line-based manifest flow (`docs/systems/asset_pipeline.md:61-79,113-120`). Javelin-4's spec is a long list of collision blocks, ramps, platforms, spawn cover, and walls (`assets/levels/javelin4.lvl:9-79`). Encounter data types can represent objective/trigger/wave concepts (`game/include/ahamkara/game/encounter_scripting.h:21-145`), but no authored encounter file for the supplied levels was identified in the inspected asset inventory.
- **Impact:** AI agents can manipulate text geometry, but lack a clear declarative path for composition, objective placement, encounter pacing, navigation constraints, difficulty tiers, and quantitative playtest. This makes level changes expensive to reason about and easy to make technically valid but unplayable.
- **Recommended fix:** Provide a versioned, validated game-level spec that references geometry, spawn groups, trigger volumes, objectives, difficulty parameters, and reward nodes. Add static checks for unreachable/overlapping spawns and missing paths, plus a top-down generated map preview and scripted/bot traversal report. Supply one complete authored level as the executable example.
- **Effort:** L
- **Dependencies:** GDES-001; rendering and AI-verification audits.

### GDES-005 — Weapon and perk catalogs are not paired with a verified balance workflow

- **Severity:** MEDIUM
- **Category:** tech debt
- **Evidence:** The runtime weapon registry defines only three weapons (AR-15, shotgun, rocket launcher) and concrete damage, fire rate, reload, ammo, and recoil values (`game/include/ahamkara/game/weapon_registry.h:10-60`). Blender-side archetypes include additional variants and stats such as aim assist, range, stability, handling, perk slots, and projectile parameters (`tools/blender/weapons/archetypes.json:1-100`). Perks have textual descriptions and stat deltas or behavior identifiers (`tools/blender/weapons/perks.json:1-100`).
- **Impact:** Designers and AI agents cannot tell from the catalog alone which values are active in runtime, whether all listed perks resolve to working effects, or whether a weapon is intentionally stronger. The divergent definition locations raise risk of tuning a value that gameplay never consumes.
- **Recommended fix:** Establish one source of truth for weapon/perk definitions, validate every declared effect against runtime support, and generate a balance report (effective DPS, time-to-kill by target health/armor, ammo economy, reload downtime, perk delta ranges). Add a small combat sandbox scenario with reproducible target and range conditions.
- **Effort:** M
- **Dependencies:** GDES-001; asset pipeline and gameplay systems audits.

### GDES-006 — Existing moment-to-moment feedback is a strong base but lacks demonstrated feel tuning

- **Severity:** MEDIUM
- **Category:** design flaw
- **Evidence:** `World::apply_damage_to_player` emits capped screen-shake and damage-flash intensity/duration, damage numbers, and a player-hit audio event (`game/src/world.cpp:920-949`). Weapon shots have deterministic recoil patterns and audio event queuing (`game/include/ahamkara/game/weapon_registry.h:23-58`; `game/src/world_projectile.cpp:51-59,283-287`).
- **Impact:** These are useful feedback channels, but without player-controlled tuning, event visualization, or hands-on playtest evidence, they may read as generic indicators rather than responsive hit/weapon feel. No inspected evidence of hitstop/slow motion, impact-specific response profiles, accessibility tuning, or a curated feel test scene.
- **Recommended fix:** Build a compact combat feel test room with scripted targets at several ranges and damage states. Expose data-driven feedback profiles for recoil, camera shake, hit flash, hit sound, and damage numbers; record parameter values and let an agent compare deterministic before/after captures. Validate with human playtest before claiming a tuned feel.
- **Effort:** M
- **Dependencies:** GDES-001; rendering/audio/input audits.

## 5. What's Good

- There is a coherent FPS/action starting direction in the shipped game code: movement, three distinct weapons, ammo/reload, recoil, damage, target dummies, abilities, and map loading are represented (`game/include/ahamkara/game/weapon_registry.h:10-60`; `game/src/world.cpp:920-989`).
- Objective vocabulary is broader than the one current horde loop: kill-all, survive, defend, collect, reach, and boss objectives plus several trigger types are explicitly represented (`game/include/ahamkara/game/encounter_scripting.h:21-105`). Preserve this flexibility while prioritizing one end-to-end experience.
- Level and weapon source data are text-based and can be edited in version control; the asset pipeline documents the manifest and compiled output model (`docs/systems/asset_pipeline.md:12-21,61-93`).
- Feedback signals include recoil, screen shake, damage flash, damage numbers, and hit audio, which are the right categories of response to tune for a shooter (`game/src/world.cpp:928-945`).
- Progression/inventory are separated into gameplay structures rather than tied to rendering, leaving room to test reward rules independently (`game/include/ahamkara/game/inventory.h:75-90`).

## 6. Open Questions for the Owner

- Which genre and target experience should govern the first quality bar? **Recommended default:** a short single-player 3D arena survival/shooter, matching the extant Flashback/FPS work.
- Is Flashback intended to be the primary reference game, or is it a renderer/tooling showcase? **Recommended default:** make Flashback the vertical slice unless it is being retired.
- Should first-slice progression persist across sessions, or reset per run? **Recommended default:** session-only upgrades first; defer meta progression until repeatable run pacing works.
- Which player platform/input setup should balance targets use? **Recommended default:** desktop keyboard/mouse at the current native-client scope, then controller pass.
- What does “fun” mean to the owner for the first slice (challenge, mastery, exploration, social play)? **Recommended default:** readable threat, responsive movement/shooting, meaningful tactical choices, and a short run with a clear win/fail/retry loop.

## 7. Handoff Notes for the Next AI Agent

- Treat all fun, feel, visual readability, difficulty, and balance conclusions as **UNVERIFIED** until someone can play the GUI build. The headless Recon results are not evidence of a playable client.
- Read `audit/01-recon.md` first; it documents the dirty tree and prevents attributing work-in-progress findings to the commit baseline.
- Inspect runtime consumers before tuning catalog data: `game/include/ahamkara/game/weapon_registry.h` and `tools/blender/weapons/*.json` are separate sources with overlapping but non-identical fields.
- The default Flashback launch is menu/Director; pass `--level <path>` for a level or `--autoplay` for the configured autoplay path, and run from repo root for relative assets (`samples/flashback/src/main.cpp:8-13,33-76`).
- Prefer a small playable loop and measured iterations over adding more activity names or progression vocabulary. Do not claim “fun” from assets, data structures, or successful compilation.

---
type: subagent-report
category: analysis
status: validated_with_known_gaps
created: 2026-06-22
agent: opencode
subsystems:
  - client
  - engine/ui
branch: main (checkpoint 43ba9cd)
validation: []
---

# Subagent Report

## Task

Plan a safe first code slice for splitting the ImGui/menu code by screen and
separating UI rendering from UI actions, without taking on the whole UI rewrite.
Task: `docs/vault/queue-tasks/claimed/TASK-20260615-1300-ui-screen-split-plan.md`.

## Status

validated_with_known_gaps (analysis-only; no code changed this slice)

## Current UI/Menu Layout (mapped)

Three responsibility layers, currently split across two big files plus a clean
state owner:

1. **State (already clean).** `ClientMenuState`
   (`client/include/ahamkara/client/client_menu_state.h`) is the single owner of
   menu visibility/screen, wrapping `ae::ui::MenuState`
   (`engine/ui/include/ae/ui/ahamkara_ui.h:20`). No duplicate screen flags. Good.

2. **Engine screens + UI runtime (`engine/ui/src/ahamkara_ui.cpp`, 795 lines).**
   - Lines 1–353: UI lifecycle (`initialize_ui`/`shutdown`/`begin`/`end`),
     capture queries, GLFW→ImGui input forwarding, and an **anonymous-namespace**
     theme palette + widget helpers (`TextCentered`, `MenuButton`,
     `SectionHeader`, `SliderSetting`, `CheckboxSetting`, `KeyBindingRow`).
   - Per-screen render functions: `render_main_menu` (359–449),
     `render_settings` (455–624), `render_character_sheet` (630–705),
     `render_pause_overlay` (711–793). Each mutates `MenuState` directly
     (`state.screen`, `state.visible`, `state.pending_*`) and/or signals via
     out-params (`quit_requested`, `quit_to_menu`) / return bool.

3. **Controller + client screens (`client/src/debug_ui_controller.cpp`, 402 lines).**
   - Routes by `menu_state_.screen()` to the engine `render_*` functions and maps
     out-params/return into `DebugUiActions` (lines 356–399).
   - **Also renders its own screens inline:** Scoreboard (118–204), Death overlay
     (209–246), Match End (251–351).
   - **Duplicates the theme + helpers:** its own `kAccent`/`kDanger`/… constants
     (17–23) and copies of `MenuButton`/`TextCentered*` (25–50) — divergent
     duplicates of the ones in `ahamkara_ui.cpp`.

## Key Findings

- **Blocker for a naive screen split:** the shared palette + widget helpers live
  in `ahamkara_ui.cpp`'s **anonymous namespace**, so a screen cannot move to its
  own translation unit until those helpers are extracted to a shared internal
  header. This is the prep the task asks about.
- **Highest-value, lowest-risk target:** the theme/widget **duplication across
  the two files** (`ahamkara_ui.cpp` vs `debug_ui_controller.cpp`). Consolidating
  it both removes a real maintenance smell and unblocks per-screen extraction.
- **Render vs actions is partially separated but leaky:** engine screens and the
  controller's inline screens mutate state and set actions inline rather than
  returning an intent. A clean boundary is a *behavior-shaped* change and should
  be its own later slice, not the first one.
- **Collision note:** `debug_ui_controller.cpp` is in `client/`, which overlaps
  the area the actively-`claimed` `client-frame-pipeline` task touches. Prefer
  starting in `engine/ui` (collision-free) and touching the controller later.

## Recommended Slice Sequence

1. **Slice 1 (prep, safe, do first):** Extract the palette + widget helpers from
   `ahamkara_ui.cpp`'s anon namespace into a shared internal header (e.g.
   `engine/ui/include/ae/ui/ui_widgets.h`, namespace `ae::ui::detail`).
   `ahamkara_ui.cpp` includes it. **Zero public-API/behavior change.** Build+test.
   - Optional follow-on within this slice: point `debug_ui_controller.cpp` at the
     same header to delete its duplicated constants/helpers (small, but touches
     `client/` — sequence after the frame-pipeline task if collision is a worry).
2. **Slice 2:** Move each engine `render_*` into its own TU under
   `engine/ui/src/screens/` (`main_menu.cpp`, `settings.cpp`,
   `character_sheet.cpp`, `pause_overlay.cpp`), each including the widgets header
   + the public `ahamkara_ui.h`. Public declarations unchanged → **no caller
   changes**. Update `engine/ui/CMakeLists.txt`. Build+test.
3. **Slice 3:** Move the controller's inline screens (Scoreboard/Death/MatchEnd)
   into their own client UI TUs using the shared widgets.
4. **Slice 4 (separate, behavior-shaped):** Formalize the render→action boundary
   — screens return a `MenuIntent`/action value instead of mutating `MenuState` +
   out-params inline; the controller maps intent → `DebugUiActions`. Treat as a
   design change with its own review.

## Why Not Do A Screen Extraction This Slice

A whole-screen move is **not yet safe** because of the anon-namespace helper
coupling (finding above). The correct first code slice is the helper/theme
extraction (Slice 1), which this report scopes precisely. Per the task, that is
the documented prep work; the first extraction is then mechanical.

## Files Changed

- None (analysis only). Recommended next-slice files: `engine/ui/` (new widgets
  header + `screens/` TUs, `CMakeLists.txt`); later `client/src/debug_ui_controller.cpp`.

## Validation Run / Results

No code changed, so build/tests are unaffected (tree builds at checkpoint
`43ba9cd`). Each recommended slice carries `cmake --build --preset debug` +
`./scripts/run-tests.sh --preset debug` and (for visible UI) a manual display run.

## Known Gaps

- Several Settings/Gameplay toggles use function-local `static` state and are NOT
  wired to `ClientConfig` (e.g. V-Sync, Show FPS, HUD/crosshair/minimap,
  hitmarkers, damage numbers in `render_settings`) — they don't persist. Worth a
  separate "wire settings to config" follow-up; out of scope here.

## Recommended Next Step

Queue/implement **Slice 1** (extract `ae::ui::detail` widgets header in
`engine/ui`) — collision-free, behavior-preserving, and it unblocks the per-screen
split.

## Confidence

high — the layout, the helper-coupling blocker, and the cross-file duplication
are all directly evidenced in the two files; the slice sequence is concrete.

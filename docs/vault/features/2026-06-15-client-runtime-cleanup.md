---
type: feature-brief
status: active
created: 2026-06-15
last_verified: 2026-06-15
subsystems:
  - client
  - engine/render
  - engine/runtime
  - docs
source_of_truth:
  - ../../client/src/debug_client.cpp
  - ../../client/src/local_play.cpp
  - ../../client/src/debug_render_runtime.cpp
  - ../../engine/render/src/debug_renderer.cpp
  - ../../docs/systems/architecture.md
---

# Client Runtime Cleanup Push

## Goal

Prepare the client/runtime/render stack for another serious feature push by
making frame ownership, client orchestration, queue-task handoff, and reporting
more explicit and easier for Codex and OpenCode to reason about.

## Non-Goals

- Full renderer rewrite
- Final production UI architecture decision
- Complete threading redesign in one pass
- Renaming every legacy debug-oriented type immediately

## Affected Systems

- `client/`
- `engine/render/`
- `engine/runtime/`
- `docs/guides/`
- `docs/vault/`

## Current Understanding

The cleanup inventory points to one central theme: too much client behavior is
spread across `debug_client.cpp`, renderer presentation semantics, UI state,
input routing, and pause/menu ownership. The first cleanup wave should carve out
 clearer ownership boundaries before adding more gameplay or rendering features.

The highest-priority items from the inventory are:

1. Clarify frame ownership and stage order.
2. Stop `DebugRenderer::render()` from implying presentation.
3. Split `debug_client.cpp` further.
4. Create a `ClientFramePipeline` or equivalent.
5. Separate debug renderer from game renderer responsibilities.
6. Remove leftover diagnostic/prototype patterns.
7. Clean up presentation vs rendering abstraction.
8. Establish one owner for pause/menu state.
9. Establish one owner for input routing.
10. Decide whether ImGui is engine UI or debug UI.

Additional cleanup areas called out in the inventory:

- client runtime ownership and lifecycle
- UI screen/component separation
- render phase separation and pass order
- input action routing and capture rules
- simulation/session state ownership
- config apply/save/dirty-state rules
- audio ownership and event flow
- scene-building boundaries
- threading/runtime ownership documentation

## Implementation Sketch

1. Capture the full cleanup inventory as a durable planning artifact.
2. Queue one narrow OpenCode task at a time through `queue-tasks/`.
3. Start with frame ownership and client frame pipeline extraction.
4. Use formal reports after each task so Codex can review evidence before
   accepting the next slice.
5. Update this brief as cleanup tasks land and priorities shift.

## Queued First Wave

The first queued OpenCode wave is:

1. `TASK-20260615-1200-client-frame-pipeline`
2. `TASK-20260615-1215-render-present-semantics`
3. `TASK-20260615-1230-pause-menu-state-owner`
4. `TASK-20260615-1245-input-routing-cleanup`
5. `TASK-20260615-1300-ui-screen-split-plan`

## Inventory Reference

Source note:

- [/Users/taufeeqali/.codex/attachments/2ef9e797-1d56-4962-99b5-358fe6093eef/pasted-text.txt](/Users/taufeeqali/.codex/attachments/2ef9e797-1d56-4962-99b5-358fe6093eef/pasted-text.txt)

## Verification

- Each queued task should define its own acceptance bar.
- For non-UI code, prefer:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

- For client frame/render/runtime changes, add relevant local run verification
  when feasible.

## Open Questions

- Should `DebugRenderer` be renamed early, or only after a cleaner pass split
  exists?
- Should `ClientFramePipeline` be a pure orchestration object or part of a new
  `ClientApplication` type?
- How much menu/pause/input ownership cleanup should happen before feature work
  resumes?

## Handoff Notes

Use `docs/vault/queue-tasks/` for implementation slices. The first recommended
task is extracting a frame pipeline and reducing `debug_client.cpp` ownership
without attempting a full renderer rename in the same pass.

Later slices should continue with presentation semantics, pause/menu ownership,
input routing, and UI screen decomposition rather than expanding the first task
into a sprawling cleanup branch.

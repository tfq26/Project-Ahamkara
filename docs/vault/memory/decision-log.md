# Decision Log

Status: Active

This is a lightweight log for decisions that future agents and humans should
notice. Keep entries short and link to fuller docs or feature briefs when they
exist.

## 2026-06-14 - Keep The Shared Vault Inside The Repo

Decision: Use `docs/vault/` as a repo-local Markdown vault that can be opened in
Obsidian and read directly by agents.

Rationale:

- Markdown stays reviewable in Git.
- Agents can read it with ordinary filesystem tools.
- Obsidian can provide graph navigation and backlinks without becoming a hard
  dependency.
- Keeping it under `docs/` makes its purpose clear while separating agent memory
  from canonical long-form docs.

Implication: Source code, tests, CMake files, and canonical docs remain the
source of truth. Vault notes explain orientation, memory, handoffs, and
decision rationale.

## 2026-06-20 - Map/Level Authoring Stack

Decision: Author maps/levels outside the engine and import them. The definite
stack is **Blender** (geometry + headless `bpy` generation), **TrenchBroom**
(fast FPS greybox/blockout), and a **custom add-on/exporter**. An in-engine
level editor is deferred.

Decision: Unify two generation paths under **one canonical JSON level spec**:

- Path A — `spec -> .lvl` (no Blender; fast iteration loop). Works with the
  existing `.lvl` -> `.aelevel` pipeline today.
- Path B — `spec -> Blender (bpy, headless) -> glTF + .lvl + .blend` (richer
  geometry + human-editable artifact).

Boundary: generation is one-way. The **spec owns layout/semantics** (mesh
placement, collision volumes, spawns, material refs); **Blender owns geometry
detail**. Hand edits to a `.blend` do not round-trip back into the spec.

Rationale: greybox/prototype levels are the agentic sweet spot; a shared spec
keeps the fast path and the rich path from diverging and enables a
describe -> spec -> generate -> import -> load loop.

Implication: `planets` are deferred and treated as scaled-up streamed maps;
build the map pipeline first. Strategic direction stays in the roadmap and
implementation work stays in GitHub Issues.

## 2026-06-20 - Defer HDR / Render-Target Foundation

Decision: Defer the HDR + offscreen render-target foundation
(`TASK-20260620-1345-render-target-hdr-foundation`). HDR is fidelity, not
capability — it improves how lighting looks but adds no new functionality — and
it is the highest-risk renderer task (rewrites the core frame loop) with no
visual validation possible in the current headless environment.

Revisit when a trigger fires:

- PBR material tuning shows clipped / washed-out highlights, OR
- bloom / SSAO / TAA is wanted (each needs an offscreen HDR target), OR
- many dynamic lights or strong emissive surfaces are added.

## 2026-06-28 - Keep HDR Deferred

Decision: Keep HDR / render-target foundation in the roadmap and decision log,
but do not treat it as current implementation work until a documented trigger
fires.

Rationale: The roadmap already tracks HDR as a late fidelity slice. Treating it
as current work causes repeated churn with no near-term execution path, so it
should return only when one of the HDR triggers in the prior decision log
entry fires.

Implication: HDR stays discoverable in the roadmap and memory notes, but
agents should not create or claim implementation work until a trigger is
confirmed in GitHub Issues.

## 2026-06-20 - Function Before Fidelity (Engine Prioritization)

Decision: Prioritize getting the engine working over visual fidelity. Near-term
order:

1. Core cleanups (`render-present-semantics`, `input-routing-cleanup`,
   `ui-screen-split-plan`) — stability and maintainability.
2. Runtime-confirm the world-render slices on a machine with a GL display
   (`runtime-confirm-prototype-levels`).
3. Make textures actually show (`textured-material-showcase`).
4. Cheap legibility wins: level-driven sky + fog (`level-driven-sky-and-fog`).
5. Only then revisit fidelity (HDR/post/lighting).

See `docs/roadmap/roadmap.md` for the full phased plan.

## 2026-06-25 - Keep Core Profile, Migrate Legacy Draws Through Compat Shim

Decision: Keep the OpenGL 3.3 core-profile client context and continue the
renderer upgrade by routing legacy-style draws through `engine/render/src/gl_compat.*`
or explicit backend helpers instead of reintroducing raw fixed-function client
state.

Rationale: The renderer already mixes shader-based passes, PBR/VAO paths, and
legacy immediate-mode-era drawing. Reverting the context to compatibility mode
would preserve old behavior but extend technical debt; migrating the remaining
draw sites makes the engine more portable and keeps the upgrade path coherent.

Implication: Any remaining direct `glEnableClientState` / `glVertexPointer`
style calls in files that include `gl_compat.h` should be treated as migration
work, not as working fixed-function code.

## 2026-06-25 - Drive Main Lighting Through Shader Uniforms

Decision: Treat the shader lighting uniforms in `debug_renderer.cpp` as the
authoritative lighting path and populate them explicitly from renderer state
each frame instead of relying on `glLight*` / `GL_LIGHTING`.

Rationale: The compat shim intentionally turns the legacy lighting calls into
no-ops under the core-profile context, so leaving those calls in place creates
a false signal that lighting is configured when it is not. Feeding uniforms
directly keeps the runtime behavior aligned with the GLSL program.

Implication: Any future renderer cleanup should update the shader uniforms or
backend abstractions directly; legacy fixed-function lighting calls should be
treated as dead code in core-profile files.

## 2026-07-13 - Separate Ahamkara, Flashback, And Wish Repositories

Decision: Ahamkara, Flashback, and Wish will become completely independent
repositories and projects.

Rationale: Ahamkara is the reusable engine, Flashback is the game, and Wish is
the backend/session platform. Independent repositories make dependency
direction, packaging, versioning, and ownership enforceable.

Implication: Ahamkara and Wish do not depend on Flashback or each other.
Flashback consumes versioned releases and owns integration adapters. See
[repository split](../../architecture/repository-split.md).

## 2026-07-13 - GitHub Issues Own Work State

Decision: GitHub Issues is the only source of truth for priority, state,
dependencies, and acceptance criteria.

Rationale: The file-backed queue duplicated issue state and made documentation
stale.

Implication: `docs/` contains durable architecture, designs, structure,
maintenance, operations, memory, and historical reports—not a second backlog.

# Owner Decisions Needed

Recommended defaults are provided so the next implementation pass can proceed once the owner confirms the baseline.

## Baseline and in-progress work

1. Which baseline should future fixes target: current `dev/flashback-menu-shell` working tree, clean `b705d74`, or the three commits ahead on `origin/main`? **Default:** preserve everything and choose a clean worktree from the commit that represents intended development.
2. What should happen to the many preexisting uncommitted changes and added assets? **Default:** preserve and inspect; do not reset, clean, or discard them. The initial audit status count cannot be reconciled exactly.
3. Is the current Flashback/Caligo work the intended reference game, or an exploratory content pass? **Default:** treat it as WIP until the owner confirms its intended milestone.

## Product scope and success

4. Which platforms must the first game and engine support? **Default:** one platform already covered by CI for M1, with cross-platform correctness retained where practical.
5. Is the intended first product 2D, 3D, or both? **Default:** use a small 3D FPS/arena slice because the current repo is strongest there; decide 2D before designing render host services.
6. What genres define “fun games” for the first year? **Default:** a short arena-defense game first; a small top-down puzzle/arena second only after the shared API is proven.
7. What observable quality bar defines a fun reference game (completion time, replayability, accessibility, human playtest criteria)? **Default:** human can understand goal without source reading, finish a short run, and report no blocking usability issue; record playtest feedback.
8. Is there a target timeline or solo-development constraint? **Default:** prioritize a thin end-to-end vertical slice and security/reproducibility gates over additional engine breadth.

## AI authoring and execution

9. Should agents author games primarily with C++, text manifests, scripts, or a mix? **Default:** declarative text for scenes/assets/objectives and trusted C++ extension points.
10. Will generated game code run unattended on a host containing credentials or private files? **Default:** consider generated code untrusted and block unattended execution until process/container isolation and a minimal environment exist.
11. Is the AI interface intended to create arbitrary games, operate Flashback for QA, or both? **Default:** both; generic authoring and verification is the product differentiator, while Flashback MCP is a QA adapter.
12. Should mutation/control APIs be available in release builds? **Default:** dev-only mutation and observation; normal games consume shipped runtime APIs.
13. Which data may be sent to external vision-QA providers? **Default:** require explicit opt-in and disclose screenshot/state transfer; offer local/offline verification for private assets.

## Runtime and content policy

14. Is general game-world save/load required for first vertical slice? **Default:** support versioned scenario snapshots for reset/replay, then general user-facing save progression later.
15. Is native scene-level 2D sprite/tilemap rendering in initial scope? **Default:** no for the first 3D slice, but do not claim 2D support until the render path is built and verified.
16. Are external assets/manifests accepted from users or agents? **Default:** yes only after canonical path confinement, checked size limits, and parser hardening; otherwise trusted-local inputs only.
17. Is the Nakama integration used with production accounts or network-accessible servers? **Default:** disable real credentials on current HTTP/UDP transports until short-lived ticket and secure transport design is implemented.
18. What target maximum world size and distinct streamed regions should drive registry scaling? **Default:** measure the intended arena/campaign bounds and keep a finite cap; do not prioritize speculative unbounded-load claims without workload evidence.

## Architecture and operations

19. Should Wish/session services stay in this repository and native build? **Default:** defer repository split; first make the engine/package boundary clean and demonstrate the out-of-tree consumer.
20. Which CI platforms should be authoritative (macOS, Windows, Linux), and is GUI screenshot capture required on each? **Default:** headless deterministic tests on all supported OSes; screenshot CI on one stable display-capable runner initially.
21. What dependency audit/update cadence and version policy is acceptable? **Default:** immutable revisions, lockfile consistency, generated SBOM, scheduled vulnerability scanning.
22. Should profiling budgets gate Debug, Release, or both? **Default:** diagnostic profiling in Debug and performance thresholds in repeatable Release-like runs on declared hardware.

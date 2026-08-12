# macOS Flashback development workflow

Status: Active

This guide defines the supported split-development workflow:

- **Local (Mac):** clone and build the Ahamkara repository, then run the
  Flashback game with a native window and display.
- **Server-side (servlenovo1):** keep headless engine, server, and pipeline
  work on the shared server so it does not depend on a Mac display.

The **Forgejo** repository
(<https://git.2helix.org/taufeeq26/Project-Ahamkara>) is the source of truth
for all branches, PRs, and validation. Do not treat any GitHub mirror as the
authoritative repository for this workflow.

## Why this split exists

Ahamkara is a C++20 engine plus an early multiplayer game codenamed
*Flashback*. The repository is a transitional monorepo that will split into
three independent repositories (Ahamkara, Flashback, Wish).
[src: file: docs/architecture/repository-split.md:1-7]

- The **client** (`client/`) and **Flashback sample** (`samples/flashback/`)
  need a window, GLFW, and OpenGL to be exercised visually.
  [src: file: samples/flashback/CMakeLists.txt:7-21]
- The **server** (`server/`) and the headless **debug-headless** preset do not
  need GLFW/OpenGL and are validated on `servlenovo1`.
  [src: file: CMakePresets.json:30-42]

The machine roles are:

| Role | Host | Runs | Purpose |
|---|---|---|---|
| Mac developer | your laptop | `debug` preset + Flashback window | Visual gameplay work, asset/level iteration |
| Headless server | `servlenovo1` (Coder) | `debug-headless` preset + CI | Engine, server, pipeline, and regression validation |

## 1. Clone the repository on the Mac

### Prerequisites

macOS toolchain requirements are the same as documented in the build guide:

```sh
brew install cmake ninja glfw
xcode-select --install
```

[src: file: docs/guides/building.md:23-34]

### Clone from Forgejo

```sh
git clone https://git.2helix.org/taufeeq26/Project-Ahamkara.git ahamkara
cd ahamkara
```

Name the remote explicitly so every push and PR targets Forgejo, never a
GitHub mirror:

```sh
git remote rename origin forgejo
git remote -v   # confirm only forgejo (git.2helix.org) is present
```

The project conventions already use a `forgejo` remote for fetch/rebase/push.
[src: file: README.md:333-351]

### Sync the Mac checkout

A clean checkout can be brought up to date with the safe sync script:

```sh
./scripts/sync-main.sh
```

The script refuses to run when the working tree has uncommitted changes and
only fast-forwards from the configured remote.
[src: file: scripts/sync-main.sh:9-33]

> Note: `sync-main.sh` currently fetches the `origin` remote. On a Mac clone
> that renames `origin` to `forgejo`, either fetch/rebase against `forgejo`
> directly (`git fetch forgejo && git rebase forgejo/main`) or keep an
> additional `origin` remote pointing at the same Forgejo URL. Never point
> `origin` at GitHub for this repository.

## 2. Configure, build, and run Flashback locally (macOS)

### Configure

From the repo root:

```sh
cmake --preset debug
```

This generates Ninja files under `build/debug` with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`.
[src: file: CMakePresets.json:8-19]

### Build

Build the full debug preset, or just the Flashback sample target:

```sh
cmake --build --preset debug
# or, to iterate on the Flashback sample only:
cmake --build --preset debug --target flashback
```

The `flashback` executable is emitted to
`build/debug/samples/flashback/flashback`.
[src: file: samples/flashback/CMakeLists.txt:19-21]

### Launch Flashback (with a display)

Use the universal launcher:

```sh
./scripts/start.sh flashback
```

or run the Flashback launcher script directly:

```sh
./scripts/run_flashback.sh
```

Both run `build/debug/samples/flashback/flashback` from the repo root so the
level's relative asset paths resolve.
[src: file: scripts/run_flashback.sh:4-10]
[src: file: scripts/start.sh:106-107]

The game boots the Ahamkara client runtime (window, renderer, input, audio)
and injects Flashback's game module. It defaults to the showcase level
`assets/compiled/levels/prototype_box.aelevel`.
[src: file: samples/flashback/src/main.cpp:23-68]

### Test locally (macOS)

The full debug suite is runnable on the Mac:

```sh
./scripts/run-tests.sh
# underlying command:
ctest --test-dir build/debug --output-on-failure
```

[src: file: scripts/run-tests.sh:49-55]
[src: file: docs/guides/building.md:66-70]

## 3. Keep headless engine/server work on servlenovo1

Server-side and pipeline work uses the **headless** preset, which disables the
GLFW/OpenGL client and the samples (`AHAMKARA_BUILD_CLIENT=OFF`,
`AHAMKARA_BUILD_SAMPLES=OFF`).
[src: file: CMakePresets.json:30-42]

### Configure, build, test (headless)

```sh
./scripts/setup-dev.sh --preset debug-headless
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

These commands run in a Coder workspace on `servlenovo1`
(<https://dev.2helix.org>) and never open a window.
[src: file: docs/guides/remote-agent-workflow.md:1-8]
[src: file: docs/guides/remote-agent-workflow.md:83-91]

### Why headless stays off the Mac

- Headless tests do not require a display, so the full engine, server, and
  gameplay regression suite runs reliably on `servlenovo1` CI even when the
  Mac is headless (for example, over SSH or an SSH session).
- The `debug` preset on the Mac is reserved for visual Flashback iteration.
- Changes that are only server/pipeline-safe are validated with
  `debug-headless` before merge; visual changes are additionally exercised
  locally on the Mac with the `debug` preset.

## 4. Push local changes to Forgejo and validate on the server

### Commit and push

```sh
git add <files>
git commit -m "Describe the change"
git push -u forgejo HEAD
```

### Open a PR against `main` on Forgejo

1. Open <https://git.2helix.org/taufeeq26/Project-Ahamkara>.
2. Open a pull request from your branch into `main` (draft until green).
3. Wait for CI to validate the branch before requesting review.

### How server validation consumes the branch

Forgejo Actions CI (`.forgejo/workflows/ci.yml`) runs on every push and PR to
`main` and verifies, from a clean checkout:

- `debug` — full client + Flashback + all tests
  [src: file: .forgejo/workflows/ci.yml:54-76]
- `debug-headless` — server/headless build and tests
  [src: file: .forgejo/workflows/ci.yml:81-102]
- `release` and `package` builds
  [src: file: .forgejo/workflows/ci.yml:107-158]

The self-hosted GitHub Actions workflow runs the same preset matrix on the
`servlenovo1` runner, including benchmarks on the headless preset.
[src: file: .github/workflows/ci.yml:64-112]

This means server validation consumes your Forgejo branch automatically: the
server runs headless tests and benchmarks without needing the Mac display or
any manual copy of local changes.

### Avoiding GitHub drift

- Clone, fetch, rebase, push, and PR **only** against the Forgejo remote.
- Do not push product branches to a GitHub mirror; the repository of record is
  Forgejo.
- If a local checkout has an `origin` remote, point it at the Forgejo URL (or
  remove it) so scripts and agents never accidentally resolve GitHub.
- Keep one active writer per branch and one branch per task.
  [src: file: docs/guides/remote-agent-workflow.md:11-16]

## Acceptance criteria mapping

| Criterion | How this guide satisfies it |
|---|---|
| Clean Mac checkout can configure and build Flashback | Section 1 + 2: clone from Forgejo, `cmake --preset debug`, `cmake --build --preset debug` |
| Flashback launches locally with a display | Section 2: `./scripts/start.sh flashback` / `./scripts/run_flashback.sh` |
| Headless tests remain runnable on the server without the Mac display | Section 3: `debug-headless` configure/build/test on `servlenovo1` |
| Forgejo is the source of truth; no GitHub drift | Section 1 + 4: single `forgejo` remote, PRs on Forgejo, CI validates the branch |

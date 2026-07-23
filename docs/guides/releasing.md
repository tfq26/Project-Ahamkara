# Release Process

This document describes how to create a new Ahamkara release using the
automated release workflow.

## Overview

Releases are triggered by pushing a version tag (e.g., `v1.2.3`) to GitHub.
The CI pipeline defined in [`.github/workflows/release.yml`](../../.github/workflows/release.yml)
handles the entire process:

1. Builds all CMake presets (debug, release, debug-headless)
2. Runs tests for presets that support them
3. Packages distributable archives (TGZ, ZIP) via CPack
4. Generates release notes from the commit log
5. Updates `CHANGELOG.md` on the `main` branch
6. Creates a GitHub Release with the packaged archives attached

## Prerequisites

- Write access to the repository.
- A working local checkout with `origin` pointing to
  `https://github.com/tfq26/Project-Ahamkara`.
- All changes intended for the release are merged into `main`.

## Creating a Release

### 1. Prepare the branch

Ensure `main` is up to date and contains everything you want to release:

```sh
git checkout main
git pull origin main
```

### 2. (Optional) Bump the version number

The project version is defined in the root
[`CMakeLists.txt`](../../CMakeLists.txt):

```cmake
project(Ahamkara VERSION <major>.<minor>.<patch> LANGUAGES CXX)
```

Update the version number if this release changes the public API or increments
the patch level:

```sh
# Example: bump to 1.0.0
sed -i 's/VERSION [0-9]*\.[0-9]*\.[0-9]*/VERSION 1.0.0/' CMakeLists.txt
```

Commit and push the version bump:

```sh
git add CMakeLists.txt
git commit -m "Bump version to 1.0.0"
git push origin main
```

### 3. Update CHANGELOG.md

Move the relevant entries from `## Unreleased` into a new versioned section.
The format follows [Keep a Changelog](https://keepachangelog.com/) conventions:

```markdown
## [1.0.0] - 2026-07-23

### Added

- New feature A.
- New feature B.

### Changed

- Existing feature C now does D.

### Fixed

- Bug E resolved.
```

Commit the changelog update:

```sh
git add CHANGELOG.md
git commit -m "Update CHANGELOG for 1.0.0"
git push origin main
```

> **Note**: The release workflow will also attempt to update `CHANGELOG.md`
> automatically with an abbreviated entry. If you have already updated it
> by hand, the workflow's commit will be a no-op (no diff).

### 4. Tag and push

Create a signed or annotated tag for the release:

```sh
git tag -a v1.0.0 -m "Ahamkara 1.0.0"
git push origin v1.0.0
```

Pushing the tag triggers the **Release** workflow on GitHub.

### 5. Monitor the workflow

Navigate to the repository's **Actions** tab:
`https://github.com/tfq26/Project-Ahamkara/actions`

Look for the running **Release** workflow. It will:
- Build and test all presets.
- Package TGZ and ZIP archives.
- Generate release notes.
- Update `CHANGELOG.md` on `main`.
- Publish a GitHub Release with artifacts.

If any step fails, inspect the logs, fix the issue on `main`, delete the tag
locally and remotely, then re-tag:

```sh
git tag -d v1.0.0
git push --delete origin v1.0.0
# fix, commit, push
git tag -a v1.0.0 -m "Ahamkara 1.0.0"
git push origin v1.0.0
```

### 6. Verify the release

Once the workflow completes:

- Check the [Releases page](https://github.com/tfq26/Project-Ahamkara/releases)
  for the new release entry.
- Verify that the TGZ and ZIP archives are attached.
- Verify that `CHANGELOG.md` on `main` has been updated.
- Download and test one of the packages locally:

```sh
tar xzf Ahamkara-1.0.0-Linux.tar.gz
ls -la Ahamkara-1.0.0-Linux/
```

## What Gets Packaged

The CPack configuration (see [`cmake/InstallRules.cmake`](../../cmake/InstallRules.cmake))
installs:

- **Libraries**: `ae_core`, `ae_network`, `ae_runtime`, and optional modules
  (collision, physics, skeleton, animation, audio, input, UI, render, platform).
- **Headers**: Public headers for all built modules.
- **CMake config**: `AhamkaraConfig.cmake` and `AhamkaraConfigVersion.cmake`
  for downstream `find_package(Ahamkara)` usage.
- **Binaries**: `ahamkara_server` (dedicated server) and `ahamkara` (client)
  if built.

Two package variants are produced:
| Variant | Contents |
|---|---|
| `Ahamkara-<version>-Linux.tar.gz` / `.zip` | Full build (client + server + engine) |
| `Ahamkara-<version>-Linux-headless.tar.gz` / `.zip` | Headless build (server + engine, no GLFW/OpenGL) |

## Release Notes

Release notes are automatically generated from the git log between the previous
version tag and the current tag. If you want to customize the notes, edit the
release on the GitHub Releases page after the workflow completes.

## Versioning Scheme

This project follows [Semantic Versioning](https://semver.org/):

- **MAJOR** — incompatible API changes.
- **MINOR** — backwards-compatible functionality added.
- **PATCH** — backwards-compatible bug fixes.

The version is defined in the root `CMakeLists.txt` and must match the tag
(prefix the version with `v`).

## Troubleshooting

| Problem | Likely Cause | Solution |
|---|---|---|
| Workflow not triggered | Tag name does not match `v*.*.*` pattern | Use `vX.Y.Z` format (e.g., `v1.2.3`). |
| Package step fails | Missing build dependencies on runner | Check the self-hosted runner has all required tools (see `building.md`). |
| CHANGELOG update fails | Permission error pushing to `main` | Ensure `GITHUB_TOKEN` has `contents: write` permission (it does in the workflow definition). |
| Release creation fails | Tag already exists on GitHub Releases | Delete the existing release and re-run. |

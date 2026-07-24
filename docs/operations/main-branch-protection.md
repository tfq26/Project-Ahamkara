# Main Branch Protection

## Purpose

The `main` branch is the release-critical integration branch. Every commit on
`main` must be buildable, testable, and packageable across all supported CMake
presets. This document describes the protection mechanisms, how to verify them,
and the authorised emergency bypass procedure.

## Protection layers

`main` is protected by two complementary mechanisms:

### 1. GitHub ruleset — "Main Protection" (primary gate)

| Property | Value |
|---|---|
| Enforcement | `active` |
| Target branches | `refs/heads/main` |
| Bypass actors | None (even administrators cannot bypass) |

**Rules enforced:**

| Rule | Effect |
|---|---|
| `deletion` | Prevents deleting the `main` branch |
| `non_fast_forward` | Blocks force pushes |
| `creation` | Prevents unintentional branch creation matching the pattern |
| `required_linear_history` | Requires a linear commit history (no merge commits) |
| `required_signatures` | Requires signed commits |
| `pull_request` | All changes must go through a PR with 1+ approving review, stale reviews dismissed, last-push approval required, review threads resolved, and only merge/squash allowed |
| `required_status_checks` | The following checks must pass before merge: **Lint**, **debug (self-hosted)**, **release (self-hosted)**, **debug-headless (self-hosted)**, **Package**. Branches must be up-to-date with `main` (`strict_required_status_checks_policy`) |

### 2. Legacy branch protection (secondary gate)

The older branch-protection rules remain configured on `main` as a fallback.
They mirror the ruleset:

- Required status checks: Lint, debug, release, debug-headless, Package
- Strict (branches must be up-to-date)
- Enforce admins
- No force pushes
- No deletions

## Required CI checks

| Check name | Trigger | What it validates |
|---|---|---|
| `Guard main` | Push to `main` (defence-in-depth) | Rejects commits containing `[skip ci]` |
| `Lint` | PR + push | CMake configure (debug), clang-tidy, clang-format, Python lint |
| `debug (self-hosted)` | PR + push | Debug build and all debug tests |
| `release (self-hosted)` | PR + push | Release (optimised) build |
| `debug-headless (self-hosted)` | PR + push | Headless debug build and headless tests |
| `Package` | PR + push | Packaging (TGZ/ZIP) |

## How merges reach `main`

```
feature branch  ──►  develop  ──►  PR to main  ──►  main
                        ▲
            agent/automerge/*
```

1. Agent branches auto-merge into `develop` via CI (only the `agent/automerge/*`
   namespace).
2. A human or release agent creates a pull request from `develop` to `main`.
3. The PR must pass all required checks and receive 1+ approving review.
4. The PR must be up-to-date with `main` before merging.
5. Only squash or merge commits are allowed (rebase is disabled).

## Verifying protection is active

### Via GitHub UI

1. Navigate to **Settings > Rules > Rulesets**.
2. Confirm "Main Protection" shows **Enforce** (green badge).
3. Navigate to **Settings > Branches**.
4. Confirm the branch protection rule for `main` shows the required checks.

### Via API

```sh
# Ruleset status
gh api repos/tfq26/Project-Ahamkara/rulesets/18999781 --jq '{name, enforcement, conditions}'

# Branch protection
gh api repos/tfq26/Project-Ahamkara/branches/main/protection --jq '.required_status_checks'
```

### Negative test

To prove protection is working, create a PR that deliberately breaks the build:

```sh
# Create a branch with a broken CMakeLists.txt
git checkout -b negative-test/break-configure origin/main
echo 'broken_command()' >> CMakeLists.txt
git add CMakeLists.txt
git commit -m "NEGATIVE TEST: deliberately break configure"
git push origin negative-test/break-configure

# Create the PR
gh pr create --base main --head negative-test/break-configure \
  --title "NEGATIVE TEST: break configure" \
  --body "DO NOT MERGE — negative test for branch protection"

# Verify the PR is blocked
gh pr view --json mergeStateStatus,statusCheckRollup
# Expected: mergeStateStatus = "BLOCKED"

# Close the PR without merging
gh pr close <number> --comment "Negative test complete. PR blocked as expected."
```

Expected result: GitHub reports the PR as **not mergeable** (blocked by failing or
pending required checks and/or the strict up-to-date requirement).

## Emergency bypass procedure

In a genuine production emergency (e.g., a security hotfix, critical service
outage), a bypass may be authorised **only** through the following process:

### Authorisation

1. **Two-person rule**: at least two maintainers must agree that the bypass is
   necessary.
2. **Issue required**: a GitHub issue must document the emergency, the proposed
   bypass, and the planned follow-up.
3. **Time-bound**: bypass access is granted for at most 4 hours.

### Steps

1. Create or link the emergency GitHub issue.
2. Add the bypass actors to the "Main Protection" ruleset via the GitHub UI
   (**Settings > Rules > Rulesets > Main Protection > Bypass actors**). Assign
   only the minimum set of actors needed.
3. Perform the merge.
4. Remove the bypass actors immediately after the merge.
5. Comment on the emergency issue with:
   - What was merged (commit SHA).
   - Why the standard process could not be followed.
   - When the bypass actors were removed.
   - A link to the follow-up issue that restores/validates the standard process.

### Audit trail

Every bypass is recorded in the GitHub Audit log (accessible under
**Settings > Security > Audit log**). The log captures which actor was added or
removed as a bypass actor and by whom.

### Follow-up

Within 72 hours of any bypass, a new issue must be opened to:

1. Investigate why the normal process could not serve the emergency.
2. Apply any necessary process or automation improvements.
3. Re-run the negative test to confirm protection is restored.

## Troubleshooting

| Symptom | Most likely cause |
|---|---|
| PR shows "BLOCKED" | Required checks are pending or failing, or branch is behind `main`. Merge or rebase to update. |
| PR shows "BEHIND" | Branch is not up-to-date with `main`. Rebase or merge `main` into the PR branch. |
| Cannot push to `main` | The `non_fast_forward` rule blocks force pushes; the `pull_request` rule blocks direct pushes. Use a PR. |
| CI does not trigger | Check that the workflow file is valid YAML. Verify GitHub Actions is enabled for the repo. |
| `[skip ci]` merge succeeded | This should no longer be possible. File a bug if it happens — the ruleset required checks independent of `[skip ci]`. |

## Change history

| Date | Change |
|---|---|
| 2026-07-24 | Initial ruleset activated with required status checks, PR requirements, linear history, and signed commits. Negative test PR #151 validated the protection (merge blocked as expected). See [docs/reports/subagents/](../reports/subagents/) for the agent implementation report. |

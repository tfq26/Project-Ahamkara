<!-- kronn:doc-version="1.0" -->
<!-- kronn:spec="https://github.com/DocRoms/Kronn/blob/main/docs/conventions/agents-md-format-v1.md" local="docs/conventions/agents-md-format-v1.md" -->
<!-- This file follows the Kronn AGENTS.md convention v1. Sections marked
curated="ai" carry [src: …] provenance per assertion. Any agent — with
or without Kronn — can read the spec at the URL above to understand the
markers and the [src:] citation grammar. -->

# Documentation Instructions For Agents

<!-- kronn:section name="anti-hallu" curated="ai" audit="2026-07-09" -->
## 0. Anti-Hallucination Protocol

You may NEVER state a non-trivial technical fact (file paths, function / API / config names, versions, behaviour, conventions) without proof. Apply this cascade — stop as soon as you have it:

1. **READ THE CODE** — Read / Glob / Grep the repo. Cite `file:line`. Source of truth #1.
2. **READ `docs/`** — siblings of this file, `conventions/`, `architecture/`, etc. Trust a doc claim only if its `[src:]` still resolves.
3. **OFFICIAL EXTERNAL DOC** — WebFetch / the relevant MCP for external libs / APIs / specs. Cite the URL.
4. **ASK THE USER** — directly, or via a focused sub-discussion. Faster than guessing.
5. **NEVER ASSERT WITHOUT PROOF** — "I don't know yet, let me check" beats a fabrication every time.

### Citation grammar (verified mechanically by Kronn when present)

Attach a structured citation to every non-trivial assertion:

- `[src: file: <path>:<line>]` — e.g. `[src: file: backend/src/lib.rs:440]`
- `[src: file: <path>:<start-end>]` — line range
- `[src: url: <url>]` — external doc
- `[src: user:<identifier>:<date>: <ref>]` — human confirmation (stable handle preferred over email; privacy by default)
- `[src: commit: <sha>]` — git commit

A citation pointing to a file/line that does not exist, or escaping the project root, is **rejected as fabricated**. A code comment is NOT authoritative — treat it as a hint to verify, never as the fact itself.

Full spec: [`docs/conventions/agents-md-format-v1.md`](conventions/agents-md-format-v1.md). **Honest by design**: `verified` means the citation *exists*, not that the claim is *true*.
<!-- kronn:section:end -->

---


These instructions apply to every file under `docs/`.

## Structure

Keep documentation organized by purpose:

- `guides/` - How to build, run, test, and operate the project.
- `systems/` - Current subsystem and architecture documentation.
- `wish/` - Wish engine protocol, runtime, and integration notes.
- `roadmap/` - Planning documents and future work.
- `reports/` - Historical reports, investigations, and subagent outputs.
- `vault/` - Obsidian-compatible agent memory. Follow `docs/vault/AGENTS.md`
  for files in that subtree.

Do not add new Markdown files directly under `docs/` unless they are indexes or
folder-level navigation files.

## Linking

- Prefer relative Markdown links.
- Link to canonical docs in `systems/` or `guides/` instead of duplicating long
  explanations.
- Historical reports may reference older paths when preserving history, but add
  current links when editing them for active use.

## Agent-Facing Docs

When adding or changing docs for agents:

- Keep the first-read path clear.
- Mark uncertain or stale information explicitly.
- Point to source files, tests, and canonical docs for verification.

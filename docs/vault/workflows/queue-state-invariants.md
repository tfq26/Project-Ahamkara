# Queue State Invariants

Status: Active

Use these invariants to keep the queue trustworthy.

## Core Rule

One task file must exist in exactly one state folder at a time:

- `open/`
- `claimed/`
- `review-needed/`
- `completed/`
- `blocked/`

No task should appear in multiple state folders at once.

## Required Transitions

- `open` -> `claimed`
- `claimed` -> `review-needed`
- `claimed` -> `blocked`
- `review-needed` -> `completed`
- `review-needed` -> `open`
- `review-needed` -> `blocked`
- `blocked` -> `open`

## File Rules

- Preserve the same task filename across all states.
- Update task frontmatter `status` whenever the folder state changes.
- Keep one current `report:` link and one current `review:` link.
- Do not leave stale copies behind in previous folders.

## Watcher Rules

- Use the task folder transition, not build output, as the completion signal.
- Treat `review-needed/` as the prompt-to-review state.
- Treat `blocked/` as the prompt-for-user-or-supervisor-input state.
- Treat `completed/` as the terminal accepted state.
- If a report exists but the task is still in `claimed/`, the worker has not
  finished the queue handshake yet.

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

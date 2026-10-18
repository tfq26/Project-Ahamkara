---
name: forgejo-issues
description: Create, inspect, and update project issues in the self-hosted Forgejo repository.
---

# Forgejo Issues

Use Forgejo at https://git.2helix.org/taufeeq26/Project-Ahamkara for all
current work state. GitHub issues are archive records only.

Use the web UI or Forgejo API with an authorized token from the environment or
secret store. Never include token values in logs, shell history, documents, or
messages. Do not use the GitHub CLI for project tracking.

Before creating an issue, search the Forgejo repository for duplicates. Preserve
the project task body structure, use existing labels and milestones, link
dependencies explicitly, and verify writes with a follow-up read. Keep state in
Forgejo; do not mirror issues into local task files.

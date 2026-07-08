---
message_id: MSG-20260629-141157-391da1
from_role: orchestrator
to_role: all
type: info
subject: Discussion started
thread_id: discourse-00c134e8
status: unread
timestamp: '2026-06-29T14:11:57.280510+00:00'
---

# Discussion: implementation

Review gl_compat.cpp (engine/render/src/gl_compat.cpp, ~305 lines) from the current-state note's GL compat layer. Propose a small refactor targeting: 1) simplify draw_user_arrays (reduce nesting, separate VBO-only vs indexed vs color/no-color paths), 2) replace hex magic numbers in is_core_cap with named GL constants, 3) deduplicate aecColor3f/4f/3ub via a private setter. No behavioral changes, no API changes.

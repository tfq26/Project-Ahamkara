# Wish Engine / Nakama boundary

Wish Engine owns the runtime-facing interfaces. Nakama owns the concrete service implementation that will later talk to auth, session, and results APIs.

## Ownership split

| Layer | Owns | Files |
|---|---|---|
| **Wish Engine** | Interface contracts and data types | `wish/core/session_services.h` |
| **Wish Engine** | Public header contract (stable API) | `wish/include/wish/core/session_services.h` |
| **Nakama integration** | No-op mock adapters | `wish/integrations/nakama/mock_session_services.h` |
| **Nakama integration** | Bridge settings and feature gating | `wish/integrations/nakama/src/nakama_bridge.cpp` + `wish/include/wish/integrations/nakama/nakama_bridge.h` |
| **Dedicated server** | Dependency wiring (interfaces only) | `server/src/dedicated_server_main.cpp` |

### Interface contracts (Wish Engine)

Three abstract interfaces are defined in `wish/core/session_services.h`:

| Interface | Method | Purpose |
|---|---|---|
| `AuthValidator` | `validate(AuthRequest) → AuthResult` | Validate a client session token or identity token |
| `SessionAdmissionService` | `admit(SessionAdmissionRequest) → SessionAdmissionResult` | Request session admission or match join after auth succeeds |
| `MatchResultReporter` | `report_match_result(MatchResult)` | Publish match outcome, final stats, or end-of-match metadata |

Each interface is a pure-virtual class so the runtime depends on the abstract contract, never on a Nakama SDK type.

### Mock implementations (Nakama integration)

`wish/integrations/nakama/mock_session_services.h` provides:

| Class | Behavior |
|---|---|
| `NoopAuthValidator` | Always accepts; derives `player_id` from endpoint, `session_id` from token |
| `NoopSessionAdmissionService` | Always admits; derives `match_id` from session_id |
| `NoopMatchResultReporter` | No-op on `report_match_result` |

### Dependency wiring (Dedicated server)

`server/src/dedicated_server_main.cpp` creates the three mock objects on the stack at startup and passes them through the session lifecycle:

1. **Auth validation** (one-time per session): When the first input arrives after handshake, `auth_validator.validate()` is called with a placeholder token and the remote endpoint.
2. **Session admission** (follows auth): `session_admission_service.admit()` is called with the auth result to bind the player to a match.
3. **Match result reporting** (at shutdown): `match_result_reporter.report_match_result()` is called if a session was admitted, with a shutdown summary.

The server links only `wish_core` (for the abstract interfaces) and includes `mock_session_services.h` for the concrete no-op types. No Nakama SDK headers appear in the server or simulation code.

## Future plug-in points

### 1. `AuthValidator::validate()` → validate session token

**Current**: No-op mock accepts everything with a placeholder `"wish-placeholder-token"`.

**Future real implementation** will:
- Receive the actual Nakama session token from the client's `ClientHelloPacket.session_token` field
- Call Nakama's session validation API (e.g., `nakama::client->authenticate_token(token)`)
- Populate `AuthResult::player_id` with the Nakama user ID or GUID
- Populate `AuthResult::session_id` with the Nakama session ID
- Set `AuthResult::accepted = false` and `error_message` for invalid/expired tokens

**Code location**: Replace `NoopAuthValidator` with a class that wraps a Nakama client handle.

### 2. `SessionAdmissionService::admit()` → request match join

**Current**: No-op mock always admits, deriving `match_id` from the session ID.

**Future real implementation** will:
- Receive the validated `player_id` and `session_id` from auth
- Call Nakama's matchmaker or match-join API (e.g., `nakama::client->join_match(match_id, player_id)`)
- Return the server-assigned `match_id`
- Set `admitted = false` if the match is full, expired, or the player is banned

**Code location**: Replace `NoopSessionAdmissionService` with a class that wraps a Nakama match handle.

### 3. `MatchResultReporter::report_match_result()` → publish match outcome

**Current**: No-op does nothing on report.

**Future real implementation** will:
- Receive `MatchResult` with `match_id`, `player_id`, `completed` flag, and outcome `summary`
- Call Nakama's match-data or leaderboard API to persist final stats
- Publish match-end metadata (scores, placements, rewards) back to Nakama
- Handle partial reports (player disconnects mid-match) vs. full match completion

**Code location**: Replace `NoopMatchResultReporter` with a class that posts match results to Nakama's API.

### 4. Bridge gating

The existing `BridgeSettings` struct and `is_enabled()` function in `nakama_bridge.h/cpp` provide a feature flag. When the Nakama bridge is disabled (default), the runtime uses no-op implementations. When enabled, the server would swap in real Nakama-backed implementations. This toggle allows the server to run without a Nakama instance during local development and testing.

## What NOT to do when adding real Nakama calls

- **Do not** include Nakama SDK headers in `wish/core/`, `wish/session/`, or `server/src/dedicated_server_main.cpp`
- **Do not** pass Nakama types through the abstract interfaces — keep Wish-native types in the request/result structs
- **Do not** add Nakama API calls inside the simulation tick loop — keep them at session lifecycle boundaries
- **Do not** make the interfaces depend on async primitives from any specific SDK — if async is needed, add a callback or future type to the Wish core contracts first

## Current runtime state

- The dedicated server wires in mock implementations and uses a placeholder token string.
- No protocol behavior changes are required yet; the seam exists so future Nakama calls can be added without pushing Nakama headers into the simulation layer.
- The server compiles and runs independently of any Nakama SDK.

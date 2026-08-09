# Wish Admin HTTP Server Architecture

Status: Current implementation plus the intended landing-page surface

This document describes the architecture of the Wish admin HTTP server: the
TCP/HTTP listener that the dedicated game server exposes for operators. It
covers the request flow, how responses are rendered/templated, how the landing
page is served, and the security posture.

Canonical source files:

- `wish/admin/admin_server.h` — `HttpAdminServer` public surface
- `wish/admin/admin_server.cpp` — request handling and rendering
- `wish/admin/server_config.h` — server/admin configuration
- `wish/admin/heartbeat_service.h` / `wish/admin/src/heartbeat_service.cpp` — server registry
- `wish/admin/metrics_collector.h` / `wish/admin/src/metrics_collector.cpp` — Prometheus metrics
- `server/src/dedicated_server_main.cpp` — integration into the dedicated server

## Role and scope

`HttpAdminServer` is the operational/admin HTTP surface of a Wish dedicated
server. It is a small, self-contained HTTP/1.1 server written directly on BSD
sockets (Winsock on Windows) rather than a third-party web framework. It runs on
a single dedicated background thread and is intentionally minimal: it exposes
status, match, player, metrics, and heartbeat-registry information as JSON, and
(once the landing-page feature lands) a human-readable HTML page at the root.
[src: file: wish/admin/admin_server.h:44-116]
[src: file: wish/admin/admin_server.cpp:24-38]

The admin server is separate from the UDP gameplay transport. The dedicated
server starts it next to the gameplay listener:
[src: file: server/src/dedicated_server_main.cpp:154-200]

```
┌─────────────────────────── dedicated server process ───────────────────────┐
│  gameplay loop (UDP 7777)   ─────  HttpAdminServer thread (TCP admin port) │
│                                   ┌──────────────────────────────────────┐ │
│  conn_manager                 │  serve() accept loop                     │ │
│  status snapshot (mutex)      │  handle_client() request/response cycle │ │
│                               └──────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

## Components

| Component | File | Responsibility |
|---|---|---|
| `HttpAdminServer` | `wish/admin/admin_server.cpp` | Listener thread, HTTP parsing, routing, response building |
| `ServerStatus` / `PlayerStatus` | `wish/admin/admin_server.h:23-42` | Snapshot of server/match/player state handed to the HTTP layer |
| `HeartbeatService` | `wish/admin/src/heartbeat_service.cpp` | Tracks game-server registrations and liveness for `/api/v1/servers` and `/api/v1/heartbeat` |
| `MetricsCollector` | `wish/admin/src/metrics_collector.cpp` | Renders `Metrics` in Prometheus text format for `/metrics` |
| `ServerConfig` | `wish/admin/server_config.h` | Runtime configuration (ports, tick rate, player cap, match length) |

`HttpAdminServer` does not own game state. The dedicated server publishes a
fresh `ServerStatus` snapshot into a shared snapshot, protected by
`admin_status_mutex`, and hands the HTTP server a `StatusProvider` callback that
reads the latest snapshot:
[src: file: server/src/dedicated_server_main.cpp:160-197]

The `MetricsProvider` callback is optional; when absent, `/metrics` renders an
empty `Metrics` snapshot.
[src: file: wish/admin/admin_server.h:46-47]
[src: file: wish/admin/admin_server.cpp:232-234]

## Server lifecycle

### `start(port, provider, heartbeat_service, metrics_provider)`

1. If already running, `start` returns `true` immediately.
   [src: file: wish/admin/admin_server.cpp:75-78]
2. Creates a TCP socket (`AF_INET`, `SOCK_STREAM`) and enables `SO_REUSEADDR`.
   [src: file: wish/admin/admin_server.cpp:85-97]
3. Binds to `INADDR_ANY` on the configured admin port and starts listening with
   a backlog of 16.
   [src: file: wish/admin/admin_server.cpp:99-116]
4. Sets `running_ = true` and spawns the `serve()` thread.
   [src: file: wish/admin/admin_server.cpp:118-120]

`start` returns `false` (with a logged error) if socket creation, bind, or
listen fails; the caller treats that as a fatal startup condition for the
dedicated server.
[src: file: server/src/dedicated_server_main.cpp:195-200]

### `serve()` accept loop

`serve()` runs on the dedicated thread until `stop()` is called:

- It calls `select()` on the listening socket with a 250 ms timeout so the
  thread can observe `running_` and exit promptly.
  [src: file: wish/admin/admin_server.cpp:148-162]
- It retries on `EINTR`, and exits on fatal socket errors.
  [src: file: wish/admin/admin_server.cpp:163-171]
- On readiness it `accept()`s one client, calls `handle_client()`, and closes
  the connection. Each connection is handled synchronously in the accept loop
  (one request per connection, no keep-alive).
  [src: file: wish/admin/admin_server.cpp:183-192]

Because handling is synchronous and single-threaded, a slow or stalled client
blocks the loop. This is acceptable for a small operator surface but is a
capacity/DoS consideration (see [Security considerations](#security-considerations)).

### `stop()`

`stop()` flips `running_`, shuts down and closes the listening socket (which
wakes the `select()` in `serve()`), and joins the thread.
[src: file: wish/admin/admin_server.cpp:123-142]

## Request flow

Every client connection follows the same `handle_client()` pipeline:
[src: file: wish/admin/admin_server.cpp:196-299]

1. **Read**: `recv()` reads at most `kRequestBufferSize` (4096) bytes into a
   buffer. A read of zero or fewer bytes is ignored.
   [src: file: wish/admin/admin_server.cpp:197-204]
2. **Parse request line**: the request is treated as the first `\r\n`-terminated
   line (`method SP path SP HTTP/version`). `parse_method()` returns the
   substring before the first space; `parse_path()` returns the path between the
   first and second spaces and strips any `?query` suffix.
   [src: file: wish/admin/admin_server.cpp:209-211]
   [src: file: wish/admin/admin_server.cpp:418-445]
3. **Gather state**: the current `ServerStatus` snapshot is fetched through the
   `StatusProvider` callback.
   [src: file: wish/admin/admin_server.cpp:213]
4. **Route**: `handle_client()` dispatches on method and path. Unknown paths get
   `404`; methods that are not allowed for a known endpoint get `405`.
   [src: file: wish/admin/admin_server.cpp:219-295]
5. **Render**: the handler builds the response body (JSON, Prometheus text, or
   HTML for the landing page).
6. **Respond**: `make_response()` writes the HTTP/1.1 status line, headers, and
   body, then `write_all()` sends the bytes and the connection is closed.
   [src: file: wish/admin/admin_server.cpp:297-298]
   [src: file: wish/admin/admin_server.cpp:301-311]
   [src: file: wish/admin/admin_server.cpp:48-67]

The server is request-line oriented: it reads a single `recv()` chunk and does
not loop to read a full request body across multiple packets. The heartbeat
`POST` body is extracted from everything after the first `\r\n\r\n`.
[src: file: wish/admin/admin_server.cpp:447-454]

## Routing table

| Method | Path | Handler | Content-Type |
|---|---|---|---|
| `GET` | `/` | Landing page (HTML info page) | `text/html; charset=utf-8` |
| `GET` | `/index.html` | Landing page (alias) | `text/html; charset=utf-8` |
| `GET` | `/health` | `render_health` | `application/json` |
| `GET` | `/match/status` | `render_match_status` | `application/json` |
| `GET` | `/players` | `render_players` | `application/json` |
| `GET` | `/api/v1/servers` | `render_servers` (heartbeat registry) | `application/json` |
| `GET` | `/api/v1/sessions` | Static empty JSON | `application/json` |
| `GET` | `/api/v1/activities` | Static empty JSON | `application/json` |
| `GET` | `/metrics` | `render_metrics` (Prometheus) | `text/plain; version=0.0.4` |
| `POST` | `/api/v1/heartbeat` | Registers a server heartbeat | `application/json` |

Any other `GET` path returns `404` with `{"error":"unknown endpoint"}`; any
other method returns `405` with `{"error":"method not allowed"}`.
[src: file: wish/admin/admin_server.cpp:238-242]
[src: file: wish/admin/admin_server.cpp:291-295]

The two landing-page rows (`/` and `/index.html`) are part of the pending
landing-page feature; on the current `main` branch those paths fall through to
`404`. See [Status relative to `main`](#status-relative-to-main).

The `/api/v1/sessions` and `/api/v1/activities` responses are static stubs;
they do not reflect live session/activity state.
[src: file: wish/admin/admin_server.cpp:228-231]

### Heartbeat endpoint

`POST /api/v1/heartbeat` accepts a JSON body with `server_id`, `address`, and
`port`. The server performs a minimal hand-rolled field extraction (it does not
link a JSON parser), validates that the three fields are present, and forwards
the record to `HeartbeatService::report_heartbeat()`.
[src: file: wish/admin/admin_server.cpp:243-290]

Validation errors produce `400` with `{"error":"missing required fields:
server_id, address, port"}`. If no heartbeat service is wired up, the endpoint
returns `503`.
[src: file: wish/admin/admin_server.cpp:278-290]

## Rendering and templating

The admin server has no template engine. Bodies are produced in-process by
string building:

- **JSON**: each render function assembles JSON with `std::ostringstream`
  (`render_health`, `render_match_status`, `render_players`, `render_servers`).
  Numeric fields are written directly; every user/state-controlled string field
  is passed through `escape_json()`.
  [src: file: wish/admin/admin_server.cpp:350-407]
  [src: file: wish/admin/admin_server.cpp:456-480]
- **Prometheus text**: `MetricsCollector::render_prometheus()` emits `# HELP`,
  `# TYPE`, and metric lines for gauges and counters in the Prometheus text
  exposition format.
  [src: file: wish/admin/admin_server.cpp:381-385]
  [src: file: wish/admin/src/metrics_collector.cpp:11-51]
- **Landing page HTML**: `render_info_page()` builds a static, self-contained
  HTML page (see [Landing page](#landing-page)). User-controlled fields are
  escaped with `escape_html()` (the HTML-context escaper), not `escape_json()`.

### Escaping

`escape_json()` escapes backslash, double quote, the C control characters
(`\b \f \n \r \t`), and other control characters as `\u00XX`, so user-supplied
strings cannot break out of a JSON string literal.
[src: file: wish/admin/admin_server.cpp:321-348]

For HTML output, JSON escaping is not sufficient: it leaves `<`, `>`, and `&`
untouched, which would allow markup injection through fields such as the game
name or player endpoints. The landing page therefore escapes the five HTML
metacharacters (`& < > " '`) with `escape_html()` before inserting
user-controlled values into the document.
[src: commit: e196335d5d3c4887313af979c5b2523b21add60d]
[src: commit: b22548536098b403f36cb969588cbdb265598cfd]
[src: commit: 5546ccb5ceeb62d0b6a72303de06a9c7faf69ce8]

## Landing page

The landing page is the human-readable root page of the admin server. It is a
server-rendered HTML document (no client-side JavaScript, no external
dependencies) that shows the current server snapshot in a browser:

- **Server status**: game name, game port, admin port, tick rate, uptime,
  server tick, max players.
- **Match**: active flag, elapsed, duration, remaining time.
- **Players**: endpoint and seconds-since-seen for each connected player.
- **API endpoints**: links to every JSON/Prometheus endpoint exposed by the
  server.

### Serving

The page is served at `GET /` and `GET /index.html` with
`Content-Type: text/html; charset=utf-8`. The response goes through the same
`make_response()`/`write_all()` path as JSON responses, and therefore inherits
`Cache-Control: no-store` and `Connection: close`.
[src: commit: e196335d5d3c4887313af979c5b2523b21add60d]
[src: file: wish/admin/admin_server.cpp:301-311]

`render_info_page()` is exposed as a public static method on `HttpAdminServer`
so it can be unit-tested directly without opening a socket.
[src: commit: e196335d5d3c4887313af979c5b2523b21add60d]

### Status relative to `main`

On the current `main` branch the root path is not yet routed and falls through
to the `404` handler:
[src: file: wish/admin/admin_server.cpp:238-242]

The landing-page implementation (routing for `/` and `/index.html`,
`render_info_page()`, and `escape_html()`) is developed on feature branches
tracked by commits `e196335`, `b225485`, and `5546ccb`. Once merged, the root
page is served as described above. Those commits also carry the landing-page
tests: `wish/admin/tests/info_page_tests.cpp` (unit coverage of
`render_info_page()` and HTML escaping) and
`tests/integration/test_landing_page.cpp` (socket-level integration that starts
the real server on a loopback port and verifies `/` and `/index.html`,
including XSS neutrality).
[src: commit: e196335d5d3c4887313af979c5b2523b21add60d]
[src: commit: b22548536098b403f36cb969588cbdb265598cfd]

## Security considerations

The admin surface is a plaintext HTTP server with no authentication layer. Its
design intent is a read-only operator/inspection surface; anyone who can reach
the admin port can read the exposed state. The mitigations that exist today:

- **Cache control**: every response carries `Cache-Control: no-store`, so status
  pages are not cached by intermediaries or browsers.
  [src: file: wish/admin/admin_server.cpp:301-311]
- **JSON injection prevention**: `escape_json()` neutralizes quotes, backslashes,
  and control characters in every string that reaches a JSON body.
  [src: file: wish/admin/admin_server.cpp:321-348]
- **HTML injection prevention**: the landing page escapes HTML metacharacters
  with `escape_html()` so user-controlled strings (game name, player endpoints)
  cannot inject markup or script into the rendered page.
  [src: commit: b22548536098b403f36cb969588cbdb265598cfd]
- **Bounded request size**: reads are capped at 4096 bytes, limiting the impact
  of oversized requests.
  [src: file: wish/admin/admin_server.cpp:27]
  [src: file: wish/admin/admin_server.cpp:197-207]
- **Connection close**: `Connection: close` avoids connection reuse and keeps
  per-request state trivial.
  [src: file: wish/admin/admin_server.cpp:301-311]
- **Fixed error bodies**: unknown routes and methods return small fixed JSON
  bodies; no stack traces or internal state are echoed to the client.
  [src: file: wish/admin/admin_server.cpp:238-242]
  [src: file: wish/admin/admin_server.cpp:291-295]

Known limitations and hardening notes:

- **No TLS**: traffic is plaintext. For anything beyond a trusted local network,
  the admin port should sit behind a reverse proxy that terminates TLS and, if
  needed, enforces authentication/authorization.
- **No authentication or rate limiting**: the heartbeat `POST` endpoint accepts
  unauthenticated registrations and could be used to pollute the server
  registry; the single-threaded accept loop could also be a DoS target. A
  per-connection or per-IP throttle plus authenticated writes would close this
  gap.
- **Binds to all interfaces**: `INADDR_ANY` means the admin port is reachable on
  every local interface. Prefer firewalling the admin port, or binding to
  loopback/trusted interfaces only.
  [src: file: wish/admin/admin_server.cpp:99-103]
- **Minimal HTTP parsing**: the parser accepts a single `recv()` chunk and does
  not implement a full HTTP state machine. It is intentionally not hardened for
  hostile traffic; it is an internal operator surface.
- **No secrets on the page**: `render_info_page()` renders only the status
  snapshot and endpoint links; it must not be extended to render tokens, keys,
  or connection details.

The Wish architecture invariant "authentication fails closed outside an
explicit development mode" applies to the session/auth boundary; the admin
surface itself is outside that boundary today.
[src: file: docs/wish/architecture.md:43-50]

## Configuration

Admin-port selection is part of `ServerConfig` and is configured through
environment variables or CLI flags:

| Env var | CLI flag | Default |
|---|---:|---:|
| `WISH_SERVER_ADMIN_PORT` | `--admin-port` | `7778` |

[src: file: wish/admin/server_config.h:13-21]
[src: file: wish/admin/server_config.h:136-182]
[src: file: docs/wish/local_run.md:12]

The docker-compose setup exposes the same port over TCP.
[src: file: docker-compose.yml:7-15]

Values that fail to parse (out of range, non-numeric) are logged and ignored,
falling back to the default.
[src: file: wish/admin/server_config.h:65-94]

## Testing

- `wish/admin/tests/heartbeat_tests.cpp` covers `HeartbeatService` (register,
  alive check, timeout, pruning).
- `tests/unit/admin/wish_admin_http_server_doc_tests.py` validates that this
  document exists and covers the required architecture topics (request flow,
  templating, landing page, security).
- On the landing-page feature branches: `wish/admin/tests/info_page_tests.cpp`
  covers `render_info_page()` across server states and HTML-escaping of
  user-controlled fields, and `tests/integration/test_landing_page.cpp` starts
  the real server on a loopback port to verify `/` and `/index.html` over the
  wire, including XSS neutrality.

See `docs/wish/local_run.md` for how to start the server and inspect endpoints
with `curl`.

## Related documents

- [Wish architecture](docs/wish/architecture.md) — module boundaries and invariants
- [Wish local run](docs/wish/local_run.md) — configuration, run, and inspection

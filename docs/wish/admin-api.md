# Wish Admin HTTP API Reference

The dedicated server exposes an HTTP/1.1 admin interface on a configurable port
(default `7778`) [src: file: wish/admin/server_config.h:14-15].
All endpoints return JSON (`Content-Type: application/json`) except `/metrics`
which returns Prometheus text format (`text/plain; version=0.0.4`).
[src: file: wish/admin/admin_server.cpp:306-316]
Responses include `Connection: close` and `Cache-Control: no-store` headers.

## Quick reference

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Server health check |
| GET | `/match/status` | Current match metadata |
| GET | `/players` | Connected player list |
| GET | `/api/v1/servers` | Registered game servers |
| GET | `/api/v1/sessions` | Session list (stub) |
| GET | `/api/v1/activities` | Activity list (stub) |
| GET | `/metrics` | Prometheus operational metrics |
| POST | `/api/v1/heartbeat` | Game server heartbeat registration |

## Common error responses

All endpoints return JSON error bodies for unrecognised paths or unsupported
methods.

**404 — Unknown endpoint**

```http
HTTP/1.1 404 Not Found
Content-Type: application/json
Connection: close
Cache-Control: no-store

{"error":"unknown endpoint"}
```

**405 — Method not allowed**

```http
HTTP/1.1 405 Method Not Allowed
Content-Type: application/json
Connection: close
Cache-Control: no-store

{"error":"method not allowed"}
```

## GET /health

Returns server liveness and configuration metadata. Intended for load-balancer
health probes and operational monitoring.

**Request**

```bash
curl -s http://localhost:7778/health
```

**Response** — `200 OK`

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Always `"ok"` on success |
| `service` | string | Server instance name (default `"Wish Engine"`) |
| `game_port` | number | UDP port the game server listens on (default `7777`) |
| `admin_port` | number | TCP port the admin HTTP server listens on (default `7778`) |
| `tick_rate` | number | Server simulation tick rate in Hz (default `60`) |

**Example**

```json
{
    "status": "ok",
    "service": "Wish Engine",
    "game_port": 7777,
    "admin_port": 7778,
    "tick_rate": 60.0
}
```

Configuration sources (first match wins): environment variables, then CLI flags
[src: file: wish/admin/server_config.h:136-159].

| Setting | Env var | CLI flag | Default |
|---------|---------|----------|---------|
| Game port | `WISH_SERVER_PORT` | `--port` / `--server-port` | `7777` |
| Admin port | `WISH_SERVER_ADMIN_PORT` | `--admin-port` | `7778` |
| Tick rate | `WISH_SERVER_TICK_RATE` | `--tick-rate` | `60` |

## Endpoint documentation template

Use this template when documenting a new admin API endpoint. Replace every
`{{placeholder}}` and remove this description paragraph.

---

### HTTP {{METHOD}} {{PATH}}

{{One-sentence description of what this endpoint does.}}

**Request**

```bash
curl -X {{METHOD}} {{http://localhost:7778/PATH}} \
  -H "Content-Type: application/json" \
  -d '{{request-body}}'
```

**Response** — `{{STATUS_CODE}} {{STATUS_TEXT}}`

| Field | Type | Description |
|-------|------|-------------|
| `{{field_name}}` | {{type}} | {{description}} |

**Example response**

```json
{{example-json-body}}
```

**Error cases**

| Status | Condition |
|--------|-----------|
| {{code}} | {{when this error is returned}} |

---

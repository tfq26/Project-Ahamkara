# Plane CE Setup & Operations

Plane CE is self-hosted at [plane.2helix.org](https://plane.2helix.org) and serves as the
project management platform for Ahamkara.

## Architecture

```
Internet → Caddy (reverse proxy) → authentik (forward auth) → Plane API (Docker Compose)
```

| Service | Address |
|---|---|
| Plane Web UI | `https://plane.2helix.org` |
| Plane API (external) | `https://plane.2helix.org/api/v1` |
| Plane API (Docker internal) | `http://plane-api:8000/api/v1` |
| Container host | `servlenovo1` |

## Authentication

Two auth methods:

| Method | When to use |
|---|---|
| **Session cookie** | Browser UI |
| **X-API-Key header** | Automation / API clients |

The API key is unique per workspace. Generate one at
**Settings → API Tokens** in the Plane UI.

**MCP clients** use the same API key via `PLANE_API_KEY` environment variable.

## Deployment

Plane runs via the official Docker Compose setup on `servlenovo1`.

```bash
# Setup (one-time)
mkdir -p /srv/docker/plane/
curl -fsSL https://raw.githubusercontent.com/makeplane/plane/master/setup/install.sh | bash
cd /srv/docker/plane

# Customize plane.env before starting:
#   WEB_URL=https://plane.2helix.org
#   PG_DB / PG_USER / PG_PWD  — reuse core Postgres stack
#   REDIS_URL / REDIS_PASSWORD — reuse core Redis stack
# Then mount Caddy route for plane.2helix.org with forward_auth to authentik.

# Start
docker compose -f docker-compose.yml up -d
```

### Container Access

Most interaction happens inside the `plane-api` container:

```bash
ssh servlenovo1
docker exec -it plane-api bash       # interactive shell
docker exec plane-api python3 /tmp/script.py   # one-off script
docker exec plane-api python3 /tmp/plane_triage_worker.py --watch  # continuous worker
```

## API Basics

Base URL: `https://plane.2helix.org/api/v1`

```
Header: X-API-Key: <your_api_key>
Content-Type: application/json
```

### Common endpoints

| Endpoint | Purpose |
|---|---|
| `GET /workspaces/{ws}/projects/{proj}/intake-issues/` | List intake items |
| `POST /workspaces/{ws}/projects/{proj}/intake-issues/` | Submit to intake |
| `PATCH /workspaces/{ws}/projects/{proj}/intake-issues/{issue_id}/` | Accept/reject/snooze |
| `PATCH /workspaces/{ws}/projects/{proj}/issues/{issue_id}/` | Update issue fields |
| `GET /workspaces/{ws}/projects/{proj}/cycles/` | List cycles |
| `POST /workspaces/{ws}/projects/{proj}/cycles/` | Create cycle |
| `GET /workspaces/{ws}/projects/{proj}/modules/` | List modules |
| `POST /workspaces/{ws}/projects/{proj}/modules/` | Create module |
| `GET /workspaces/{ws}/projects/{proj}/labels/` | List labels |
| `GET /workspaces/{ws}/projects/{proj}/states/` | List states |

### Intake Lifecycle

The intake pipeline uses a wrapper record (`IntakeIssue`) around the actual
issue. Key status values for `IntakeIssue.status`:

| Value | Meaning |
|---|---|
| `-2` | Pending (needs triage) |
| `-1` | Rejected |
| `0` | Snoozed |
| `1` | Accepted |
| `2` | Duplicate |

Workflow:
1. A ticket arrives in the project's Triage state, creating an `IntakeIssue` record with `status=-2`.
2. The triage worker inspects the issue while still in Triage, assigning labels, priority, module, and cycle.
3. Accepting (`PATCH .../intake-issues/{issue_id}/` with `{"status": 1}`) transitions the issue from Triage to Backlog.
4. The PATCH endpoint takes the **Issue ID** (UUID from `issue_detail.id`), not the IntakeIssue ID.

### Important API quirks

- **State field**: The API field is `"state"` (UUID), not `"state_id"`.
- **Cycle creation**: Requires `"project_id"` in the request body (validated by `CycleCreateSerializer`).
- **Module creation**: Module validation reads `project_id` from the view context, so it cannot be set in the request body.

## Intake Triage Worker

Script: `tools/plane-triage/plane_triage_worker.py`

An automated triage worker that processes pending intake items:

```bash
# Single run
docker exec plane-api python3 /tmp/plane_triage_worker.py

# Continuous polling (every 60s)
docker exec plane-api python3 /tmp/plane_triage_worker.py --watch
```

Copy the script to the server and into the container:

```bash
scp tools/plane-triage/plane_triage_worker.py servlenovo1:/tmp/
ssh servlenovo1
docker cp /tmp/plane_triage_worker.py plane-api:/tmp/
```

### How it works

1. Fetches all pending intake items (`status=-2`)
2. Classifies each issue using keyword matching against name + description HTML
3. Assigns priority, labels, module, and cycle while the issue is still in Triage
4. Accepts the intake item (moves issue from Triage to Backlog)

### Configuration

The script uses these environment variables:

| Variable | Default | Description |
|---|---|---|
| `PLANE_BASE` | `http://172.18.0.29:8000/api/v1` | Plane API base URL |
| `PLANE_API_KEY` | `ec945f9d2896435591989c968eb4c341` | Workspace API token |

### Classification Rules

Priority keywords:

| Priority | Keywords |
|---|---|
| `urgent` | crash, critical, security, data loss, blocker, p0, sev0 |
| `high` | bug, broken, fail, error, regression, p1, sev1 |
| `medium` | improve, refactor, optimize, p2, sev2 |
| `none` | (default) |

Label detection matches 18 label patterns against issue text (e.g. `engine/render` matches
render/graphics/gpu/shader/vulkan/DX12/metal/lighting/shadow).

Module assignment cascades in priority order: Rendering > Animation & Audio > Engine Core >
Physics & Collision > Networking > Tools & SDK > Governance.

### Available Modules

| Module | Labels |
|---|---|
| Engine Core | `engine/core`, `engine/runtime` |
| Rendering | `engine/render` |
| Physics & Collision | `engine/physics`, `engine/collision` |
| Networking | `engine/network` |
| Tools & SDK | `tools` |
| Animation & Audio | `engine/animation`, `engine/audio` |
| Governance | `docs`, `tracking` |

## Importing Issues from GitHub

The GitHub-to-Plane import maps each repo issue to a Plane issue. Closed issues
are restored to Done or Cancelled states. Labels are created in Plane automatically
before import.

```bash
# Fetch issues from GitHub
gh issue list --repo tfq26/Project-Ahamkara --state all --limit 200 --json \
  number,title,body,state,labels,createdAt,updatedAt,closedAt > /tmp/gh_issues.json

# Run the import script (scp'd into the container)
docker exec plane-api python3 /tmp/import_to_plane_server.py
```

## MCP Integration

The [Plane MCP Server](https://www.npmjs.com/package/@makeplane/plane-mcp-server)
provides tool-based access to Plane for AI agents. Register it in your MCP
configuration:

```json
{
  "mcpServers": {
    "plane": {
      "command": "npx",
      "args": ["-y", "@makeplane/plane-mcp-server"],
      "env": {
        "PLANE_API_KEY": "<your-api-key>",
        "PLANE_HOST_URL": "https://plane.2helix.org",
        "PLANE_WORKSPACE_SLUG": "projects"
      }
    }
  }
}
```

Available MCP tools: `get_projects`, `create_issue`, `list_project_issues`,
`get_issue_using_readable_identifier`, `list_cycles`, `list_modules`,
`list_labels`, `list_states`, `list_issue_types`, `get_workspace_members`,
`get_user`.

## Ahamkara Project

| Property | Value |
|---|---|
| Project ID | `d491cc85-ce1e-4bfd-a8c0-67d7dbaebd5e` |
| Workspace slug | `projects` |
| Labels | 28 |
| Issues | ~88 (43 Backlog, 33 Done, 12 Cancelled) |

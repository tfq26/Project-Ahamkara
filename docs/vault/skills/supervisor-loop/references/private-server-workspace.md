# Private Server Workspace

Use this reference when hosting the agent organization on a private server that the user can SSH into from anywhere.

## Recommended Architecture

Use a simple, inspectable setup first:

- Linux server with key-only SSH
- a non-root user for daily work
- project repositories under `~/projects/`
- Obsidian agent vault under `~/agent-vault/`
- skills under `~/skills/` or the agent's configured skills path
- `tmux` sessions for long-running agent work
- Git for project and vault history
- a private network layer or restricted firewall for remote access

Avoid exposing dashboards or web IDEs publicly until authentication, TLS, backups, and update processes are clear.

## Access Pattern

Prefer one of these:

- SSH over a private VPN or mesh network
- SSH with key-only auth, firewall restrictions, and fail2ban-style protection
- SSH through a bastion host if the server sits inside a private network

Disable password login. Keep root login disabled for normal use.

## Workspace Layout

Example:

```text
~/projects/
  app-one/
  app-two/
~/agent-vault/
  00-Control/
  01-Roster/
  02-Dashboard/
  03-Tasks/
  04-Reports/
  05-Decisions/
~/agent-runs/
  logs/
  sessions/
~/skills/
  supervisor-loop/
  subagent-reporting/
  subagent-collaboration-protocol/
```

Keep the Obsidian vault separate from project repositories unless a project intentionally owns its own team workspace.

## Agent Runtime

Start with cooperative processes:

- one `tmux` session per agent or group
- each agent receives its role, task ID, project path, and vault path
- each agent reads control files before each work step
- each agent writes reports and heartbeats

Later, add a runner that can:

- spawn agents from task board entries
- stop or pause processes by agent ID
- collect logs
- update heartbeat fields
- enforce max runtime and max loop counts

## Obsidian Access

Options:

- edit the vault locally by syncing Markdown files from the server
- mount the vault over SSHFS from a trusted machine
- use Git to pull/push vault changes
- run a private web file editor only behind strong auth or VPN

Do not require Obsidian to be installed on the server. The server only needs to store Markdown files.

## Backups

Back up:

- project repositories
- Obsidian vault
- agent reports and logs
- shell profiles, SSH config, and skill folders

Use at least one off-server backup. Test restore before relying on it.

## Security Rules

- Keep secrets out of task notes and reports.
- Use `.env` files or secret managers for credentials.
- Limit agent access to only the project paths they need.
- Review generated commands before running privileged operations.
- Keep OS and runtime dependencies updated.
- Log agent actions enough to reconstruct what happened.

## Setup Sequence

1. Provision the server.
2. Create the user account and SSH keys.
3. Configure firewall and private access.
4. Create workspace directories.
5. Install core tools: Git, tmux, language runtimes, and agent CLIs.
6. Copy or clone skills.
7. Create the Obsidian vault from `assets/obsidian-agent-vault/`.
8. Start one supervisor session.
9. Start one worker session on a small test task.
10. Verify stop, steering, report, Kanban, and final synthesis flow.

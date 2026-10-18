# 11 — Security & Safety Audit

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed state: dirty development checkout documented in [01-recon.md](01-recon.md).
Overall security rating: **2/5 — no obvious committed credential found in the targeted scan, and several useful opt-in/fail-closed choices exist, but native game code and asset tooling have no hostile-input isolation boundary.**

## 1. Scope & Method

Read `audit/01-recon.md`, inspected native runtime/build flags, game script references, asset manifest/importer and glTF loader paths, local Game MCP bridge, Wish Nakama HTTP auth bridge, error redaction/support bundle code, CMake FetchContent pins, package manifests/lockfiles, `.gitignore`, and example environment files. Used read-only `rg`, `nl`, and `sed` inspections. No build, test, package audit, network lookup, dynamic fuzzing, or source/config modification was performed in this specialist pass. Recon records that outbound GitHub DNS was unavailable and the offline pnpm frozen install failed for absent metadata; therefore **dependency CVE status is UNVERIFIED**. The secret scan looked for credential-like files and patterns, but was not a full historical Git secret scan or credential-provider audit. Findings describe attack surfaces evidenced by code; exploitability depends on who can supply the asset/config/script and which tools the owner runs.

## 2. Executive Summary

- **Overall rating: 2/5.** This is a native C++ application architecture, not a sandboxed runtime for untrusted AI-generated code. The module contract is an in-process C++ interface and exposes no capability restriction or process isolation. [src: file: engine/runtime/include/ae/runtime/game_module.h:35-48]
- No gameplay Lua interpreter/executor was found in the inspected tree. `LuaScriptedBehavior` is a gameplay enum/comment, while CMake’s optional Lua/sol2 fetch is described as a Wish admin HTTP server dependency. Treat game scripting as **MISSING/UNVERIFIED**, not sandboxed. [src: file: game/include/ahamkara/game/item_registry.h:67-79] [src: file: CMakeLists.txt:76-94]
- Asset manifest paths allow absolute paths and `..`; importer copies/compiles to the resolved output directly. A supplied manifest can read/copy inputs and overwrite paths outside the project. [src: file: tools/asset_importer_common.cpp:53-60] [src: file: tools/asset_importer_dispatch.cpp:35-63]
- glTF external buffer URIs are concatenated with the glTF directory and opened without root confinement. The custom parser has unbounded recursive nesting and unbounded whole-file/vector allocation; malformed or hostile files can read unintended local files or exhaust/crash the importer. [src: file: engine/render/src/gltf_loader.cpp:346-354] [src: file: engine/render/src/gltf_loader.cpp:439-463] [src: file: engine/render/src/gltf_loader.cpp:623-630]
- The optional Nakama bridge rejects HTTPS but sends the session token as a Bearer header over a plain socket when configured with HTTP. Do not use it with real credentials across an untrusted network. [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:172-179] [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:298-305] [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:349-377]
- Game MCP is compile-time opt-in, limited to Debug/RelWithDebInfo definitions, requires enable/root/token environment values, and accepts a narrow input-command grammar with bounded duration. These controls reduce exposure but do not make a shared writable command directory safe against local attackers or provide OS sandboxing. [src: file: CMakeLists.txt:116-120] [src: file: client/src/game_mcp_bridge.cpp:92-112] [src: file: client/src/game_mcp_bridge.cpp:145-220]
- CMake pins Jolt, GLM, EnTT and sol2 tags, but miniaudio and optional Lua use moving `master`. No vulnerability database check was possible. [src: file: CMakeLists.txt:34-69] [src: file: CMakeLists.txt:76-94]
- Targeted working-tree scan found environment variable references and example config files, but no actual credential value was verified. This does not certify Git history, ignored local files, CI settings, or developer machines are secret-free. [src: file: .env.example:1-7] [src: file: backend/.env.example:1-3]

## 3. Inventory

| Component | Location (paths) | Status (IMPLEMENTED/PARTIAL/STUBBED/PLANNED/MISSING) | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Game code/script isolation | `engine/runtime`, `game`, build/runtime process | MISSING | 1 | Native C++ module runs in host process; no sandbox contract found. |
| Gameplay script execution | `game/include/ahamkara/game/item_registry.h`, `CMakeLists.txt` | MISSING | 1 | Lua behavior label exists; no gameplay interpreter API found; optional Lua fetch is for Wish admin. |
| Asset manifest confinement | `tools/asset_importer_*` | MISSING | 1 | Absolute and parent-relative paths accepted; writes are not constrained to an approved root. |
| glTF hostile-input handling | `engine/render/src/gltf_loader.cpp` | PARTIAL | 1 | Custom parser/load path has no visible file-size/depth cap or URI root confinement. |
| Game MCP local bridge | `client/src/game_mcp_bridge.cpp`, CMake options | PARTIAL | 3 | Opt-in and token-gated, but filesystem transport permissions/symlink policy are not enforced here. |
| Nakama auth transport | `wish/integrations/nakama/src/nakama_bridge.cpp` | PARTIAL | 1 | HTTP only; HTTPS URLs fail closed, but HTTP can send a real bearer token in clear text. |
| Dependency integrity | `CMakeLists.txt`, npm/pnpm manifests and locks | PARTIAL | 2 | Several tags pinned, two `master` references; vulnerability audit unavailable. |
| Secret hygiene | `.env.example`, `backend/.env.example`, `.gitignore`, error/support reporting | PARTIAL | 3 | Examples and some redaction support exist; this was not a full history/CI scan. |
| Error redaction | `engine/core/src/error_report.cpp` | PARTIAL | 3 | Sensitive context keys and some bearer/JWT-like values are redacted before logs; heuristic coverage is incomplete. |

## 4. Detailed Findings

### SEC-001 — Native game modules execute with host process privileges

- **Severity:** BLOCKER
- **Category:** security
- **Evidence:** `IGameModule` is a native C++ virtual interface initialized and ticked directly by `Application`; its host services contain only a mode and opaque pointer, with no capability policy or sandbox boundary. [src: file: engine/runtime/include/ae/runtime/game_module.h:24-48] [src: file: engine/runtime/src/application.cpp:61-76] CMake links game/client code into native libraries and executables rather than launching a restricted worker. [src: file: game/CMakeLists.txt:48-59] [src: file: client/CMakeLists.txt:10-42]
- **Impact:** Any generated or third-party game code compiled into the host can use the process’s filesystem, network, credentials, and OS privileges. “AI-native” authoring is unsafe for unattended or untrusted code execution unless the surrounding environment supplies isolation.
- **Recommended fix:** Declare native game code trusted-only. For untrusted AI output, execute in a separate restricted process/container with a narrow RPC API, read-only or staged project mounts, explicit network policy, resource/time limits, and kill/restart behavior. Do not treat an embedded scripting VM as a sandbox unless dangerous libraries and resource limits are explicitly controlled.
- **Effort:** XL
- **Dependencies:** none

### SEC-002 — Asset manifest paths can escape the project and overwrite arbitrary files

- **Severity:** HIGH
- **Category:** security
- **Evidence:** `resolve_path` preserves absolute paths and lexically normalizes `manifest_dir / token`; it does not check that the result remains beneath a trusted root. [src: file: tools/asset_importer_common.cpp:53-60] The manifest accepts source, output, and metadata paths directly from tokens. [src: file: tools/asset_importer_manifest.cpp:19-51] The importer then creates output directories and copies with `overwrite_existing`. [src: file: tools/asset_importer_dispatch.cpp:35-63]
- **Impact:** Running the importer on an AI-generated or otherwise untrusted manifest can read/copy arbitrary local source files and overwrite arbitrary writable files, including files outside the repository. This can turn a routine asset import into data disclosure or destructive modification.
- **Recommended fix:** Resolve canonical paths against an explicit project/import root and reject paths outside it (including symlink escapes); separate trusted output roots from source roots; reject source/output aliasing; write to temporary files then atomically replace only approved destinations. Add adversarial tests for absolute paths, `..`, symlinks, and output equal to source.
- **Effort:** M
- **Dependencies:** none

### SEC-003 — glTF import permits external-file reads and unbounded hostile input

- **Severity:** HIGH
- **Category:** security
- **Evidence:** glTF buffers load from `base_path + uri` with no canonical root check. [src: file: engine/render/src/gltf_loader.cpp:623-630] `read_file` sizes and allocates the entire referenced file. [src: file: engine/render/src/gltf_loader.cpp:346-354] The root JSON input is also read fully into a string, and recursive array/object parsing has no visible input/depth budget. [src: file: engine/render/src/gltf_loader.cpp:439-463] [src: file: engine/render/src/gltf_loader.cpp:143-176]
- **Impact:** An untrusted model can make the importer read files outside its asset directory and consume excessive memory/stack. If imported output is later inspected or shared, unintended file contents may be reflected through compiled model data. Crafted malformed numeric input can also throw from `std::stod` in the parser without a local catch. [src: file: engine/render/src/gltf_loader.cpp:95-111]
- **Recommended fix:** Canonicalize every external URI under an approved asset root; define accepted URI forms and reject absolute paths, parent traversal, and unsupported schemes/data URIs explicitly. Add limits for JSON bytes, nesting depth, buffers, individual/aggregate buffer bytes, array counts, and numeric ranges. Convert parse exceptions to structured errors and fuzz the parser with malformed files.
- **Effort:** M
- **Dependencies:** SEC-002

### SEC-004 — Nakama bearer tokens may be sent over plaintext HTTP

- **Severity:** HIGH
- **Category:** security
- **Evidence:** `build_http_request` puts the supplied session token in an `Authorization: Bearer` header. [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:172-179] `perform_account_lookup` rejects TLS mode as unsupported but proceeds with socket connect/send otherwise. [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:298-305] [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:324-360]
- **Impact:** Network observers or a compromised intermediary can capture a live session token and impersonate its user. This is especially dangerous because settings parse both HTTP and HTTPS URL forms, while HTTPS currently returns an error only after reaching the lookup function. [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:136-149]
- **Recommended fix:** Make TLS mandatory for non-loopback hosts, implement verified TLS with certificate/hostname validation, or refuse to send real tokens except to explicitly configured loopback development endpoints. Never silently downgrade `https` to plaintext. Add tests proving TLS configuration fails closed before socket creation until a secure client exists.
- **Effort:** M
- **Dependencies:** none

### SEC-005 — Dependency vulnerability and provenance status is unknown

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** CMake uses moving `master` for miniaudio and optional Lua, while the other listed Git FetchContent dependencies use version tags. [src: file: CMakeLists.txt:34-69] [src: file: CMakeLists.txt:76-94] Recon reports outbound GitHub DNS failure and an offline pnpm frozen install failure due to missing cached package metadata, so no fresh install or vulnerability database audit completed. [src: file: audit/01-recon.md:24-37]
- **Impact:** Builds can resolve different source revisions over time for moving branches, weakening reproducibility and making incident response/version attribution harder. Actual known CVEs and transitive package risks remain **UNVERIFIED**; this report does not claim a specific dependency is vulnerable.
- **Recommended fix:** Pin all source dependencies to immutable commits or verified releases with hashes; run an online software composition analysis for CMake and npm/pnpm dependencies once network is available; preserve an SBOM and review/update schedule in CI.
- **Effort:** S
- **Dependencies:** none

### SEC-006 — Local Game MCP trusts filesystem permissions and stores a reusable token in each command

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** The bridge chooses its root from environment configuration, writes ready/state/frame files there, and scans a `commands` directory. [src: file: client/src/game_mcp_bridge.cpp:92-134] Each command file includes a token compared directly with the configured token; successful/invalid command files are removed. [src: file: client/src/game_mcp_bridge.cpp:145-220] Capture requests also contain the token. [src: file: client/src/game_mcp_bridge.cpp:298-305]
- **Impact:** Anyone able to read the shared command directory can recover the reusable token; anyone able to write there can issue gameplay input while the bridge is active. The bridge’s file protocol is useful for local agents but relies on OS directory access control and trusted local processes.
- **Recommended fix:** Create the root and command directory with owner-only permissions, reject symlinked root/command entries, ensure the root is a dedicated temporary directory, avoid writing reusable tokens into command files (use a per-run nonce/HMAC or inherited local pipe), and document that the bridge must not run in shared/untrusted workspaces.
- **Effort:** M
- **Dependencies:** SEC-001

### SEC-007 — Error redaction is useful but heuristic and support bundles can include diagnostics

- **Severity:** LOW
- **Category:** risk
- **Evidence:** Error logging redacts values when context keys contain token/password/secret/authorization/cookie/ip and redacts bearer/JWT-like strings or IPv4-like strings. [src: file: engine/core/src/error_report.cpp:18-54] The formatter logs context values after that heuristic. [src: file: engine/core/src/error_report.cpp:91-108] Support bundles include failure diagnostics and owner-provided notes, but the current environment key list contains only selected non-secret Wish server/Nakama settings. [src: file: wish/core/src/support_bundle.cpp:83-109] [src: file: wish/core/src/support_bundle.cpp:155-194]
- **Impact:** Key-name and string-shape heuristics can miss credentials under unexpected field names or formats; diagnostic text and notes may contain sensitive data. The support bundle shown here lists key names, not environment values, which is a good minimization choice.
- **Recommended fix:** Prefer allowlisted diagnostic fields and structured secret types over broad strings; centralize redaction with tests for tokens, URLs with embedded credentials, IPv6/IPv4, cookies, and arbitrary secret formats. Treat bundle notes as untrusted/sensitive and preview before export.
- **Effort:** S
- **Dependencies:** none

### SEC-008 — Admin HTTP status and heartbeat routes are unauthenticated on all interfaces

- **Severity:** HIGH
- **Category:** security
- **Evidence:** `HttpAdminServer::start` binds its listener to `INADDR_ANY` and starts accepting connections without a bind-address parameter. [src: file: wish/admin/admin_server.cpp:76-121] The request handler exposes status, player, metrics, and server-list GET routes without an auth check, and accepts unauthenticated `POST /api/v1/heartbeat`. [src: file: wish/admin/admin_server.cpp:197-305] That heartbeat mutates the in-memory server registry using caller-supplied ID/address/port, which is later returned by server-list handling. [src: file: wish/admin/src/heartbeat_service.cpp:11-42] The dedicated server starts this listener on its configured admin port. [src: file: server/src/dedicated_server_main.cpp:154-199]
- **Impact:** Any network peer that can reach the admin port can inspect player endpoint/status data and inject or refresh arbitrary heartbeat records. The endpoint is plain HTTP and has no authentication or network allowlist.
- **Recommended fix:** Bind loopback by default and expose a configurable private bind address. Require authentication/authorization for the heartbeat and operational endpoints; validate heartbeat callers and cap request sizes. Document reverse-proxy/private-network deployment requirements before public exposure.
- **Effort:** M
- **Dependencies:** admin-server configuration and deployment model.

### SEC-009 — Game ClientHello transmits the Nakama bearer token over UDP

- **Severity:** HIGH
- **Category:** security / credential transport
- **Evidence:** The headless client loads the Nakama token, copies it into `ClientHelloPacket::auth_token`, and serializes/sends that packet during connection setup. [src: file: client/src/headless_clients.cpp:400-423] [src: file: client/src/headless_clients.cpp:437-440] `NetworkSimulator::send_to` uses the ordinary UDP socket send path. [src: file: engine/network/src/udp_socket.cpp:133-160]
- **Impact:** The Nakama credential is observable to passive network monitors and intermediary hosts on the route to the game server. This remains true even if backend validation is later moved to HTTPS; the client-to-game handshake itself carries the reusable credential in plaintext.
- **Recommended fix:** Do not place a long-lived Nakama bearer token in gameplay datagrams. Exchange it for a short-lived, audience-bound game ticket over a secure authenticated channel and add replay protections, or secure the game transport with an authenticated encryption protocol such as DTLS.
- **Effort:** L
- **Dependencies:** game authentication protocol and transport design.

### SEC-010 — Compiled texture dimensions can overflow the payload-size check

- **Severity:** HIGH
- **Category:** security / malformed asset handling
- **Evidence:** `CompiledTextureLoader::load` reads untrusted 32-bit dimensions and computes `width * height * 4` in `size_t` without checking dimension bounds or multiplication overflow, then compares the result to a 32-bit payload length and resizes only to that payload length. [src: file: engine/render/src/compiled_texture.cpp:91-116] The level renderer passes loaded dimensions and pixel data to the backend. [src: file: engine/render/src/level_render.cpp:135-151] The OpenGL backend submits those dimensions and pixel pointer to `glTexImage2D`. [src: file: engine/render/src/render_backend_opengl.cpp:301-312]
- **Impact:** Crafted dimensions can wrap the expected byte calculation so a too-short payload is accepted; subsequent GPU upload may request more bytes than the vector contains or provoke driver allocation failure. Other large dimensions can also cause resource exhaustion.
- **Recommended fix:** Reject zero or over-limit dimensions, use checked multiplication before computing byte count, require an exact payload length within a strict maximum, and report a stable loader error before GPU allocation. Add malformed-dimension and overflow test fixtures.
- **Effort:** S
- **Dependencies:** supported texture-size limits and renderer device caps.

### SEC-011 — Agent-runner workers inherit secrets and have no OS sandbox

- **Severity:** MEDIUM
- **Category:** security / generated-code execution
- **Evidence:** `worker_env` starts from a full copy of the invoking environment and only adds task metadata. [src: file: tools/agent-runner/agent-runner.py:559-583] The runner executes the operator-configured worker command with `shell=True` and sets the worktree only as its current directory. [src: file: tools/agent-runner/agent-runner.py:586-622] Worktree creation isolates Git changes but does not add process or filesystem restrictions. [src: file: tools/agent-runner/agent-runner.py:625-641]
- **Impact:** A worker processing untrusted task content or generated code can read credentials in environment variables, access the user's filesystem and use available network/process permissions. The shell command is operator-configured, so this finding concerns worker isolation and credential exposure, not an input-to-shell injection claim.
- **Recommended fix:** Run generated-code workers in a restricted container/OS sandbox with a minimal allowlisted environment, network policy, resource/time limits, and narrowly mounted writable directories. Prefer argv-based launch over `shell=True`; document the trust assumption if workers intentionally run with full developer privileges.
- **Effort:** M
- **Dependencies:** supported local agent runtime and credential-access needs.

### SEC-012 — Game QA sends screenshots and state to a configurable external endpoint

- **Severity:** MEDIUM
- **Category:** privacy / outbound data flow
- **Evidence:** `VisionQaClient` reads its API key and destination base URL from environment variables (default destination is OpenRouter). [src: file: tools/game_qa_model.py:44-53] On explicit analysis, it sends the framebuffer image, serialized game state, user prompt, and Bearer API key in an HTTP request. [src: file: tools/game_qa_model.py:54-89] The Game MCP exposes this through a dedicated `game_analyze_frame` tool. [src: file: tools/game_mcp_server.py:201-204]
- **Impact:** QA calls transfer rendered project content and runtime state to a remote provider; changing `OPENROUTER_BASE_URL` also redirects the API key and payload. This is an explicit tool action rather than an automatic upload, but the transfer and destination should be understood before analyzing private or unreleased assets.
- **Recommended fix:** Make the destination and data categories explicit in configuration/docs, require HTTPS for custom base URLs, validate allowed schemes/hosts, and provide a local/offline mode for sensitive assets. Avoid including secrets or unrelated state in prompts and observations.
- **Effort:** S
- **Dependencies:** selected QA provider and privacy requirements.

## 5. What's Good

- The game MCP bridge is disabled by default, is compile-time optional, and root CMake rejects a configured non-Debug/non-RelWithDebInfo build type. [src: file: CMakeLists.txt:116-120]
- Game MCP refuses to start when required root or token environment settings are absent. [src: file: client/src/game_mcp_bridge.cpp:92-108]
- MCP input parsing uses a narrow command vocabulary, validates numeric/bool fields, clamps weapon slot, and bounds requested command duration to 1–600 ticks. [src: file: client/src/game_mcp_bridge.cpp:145-220]
- The Nakama bridge rejects HTTPS mode rather than silently attempting TLS incorrectly, and its validator factory denies authentication by default outside explicit development mode when the integration is disabled. [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:298-305] [src: file: wish/integrations/nakama/src/nakama_bridge.cpp:456-468]
- Error reporting applies redaction before logging sensitive-looking context keys and bearer/JWT-like values. [src: file: engine/core/src/error_report.cpp:18-54] [src: file: engine/core/src/error_report.cpp:98-108]
- `.env.example` and `backend/.env.example` contain defaults/placeholders rather than observed credentials, and `.gitignore` excludes `.mcp.json` and common local build outputs. [src: file: .env.example:1-7] [src: file: backend/.env.example:1-3] [src: file: .gitignore:1-35]
- No actual credential value was confirmed by the targeted working-tree scan; treat this only as limited scan evidence, not a security certification.

## 6. Open Questions for the Owner

- Will AI-generated native C++ ever run automatically on the owner’s workstation or CI? **Recommended default:** assume all generated native code is untrusted and use a separate restricted process before enabling unattended execution.
- Are asset manifests and glTF sources allowed to come from external users or generative agents? **Recommended default:** treat them as hostile and enforce filesystem roots and resource budgets.
- Is the built-in Nakama bridge used against production accounts? **Recommended default:** no; disable it for production until TLS with certificate validation is implemented.
- Is Game MCP expected to run in shared workspaces or CI? **Recommended default:** only in owner-controlled isolated workspaces with a fresh root and token per run.
- Which dependency scan and update cadence should be mandatory? **Recommended default:** CI-generated SBOM plus scheduled vulnerability scan and immutable source pins.

## 7. Handoff Notes for the Next AI Agent

- Read `audit/01-recon.md` first. The working tree is heavily dirty; preserve all pre-existing changes and write audit deliverables only under `/audit/`.
- Do not describe the gameplay as Lua-sandboxed: no runtime execution path was found. The `LuaScriptedBehavior` label does not prove executable Lua exists.
- Treat `tools/ahamkara_asset_importer` and the glTF loader as trusted-input tools until SEC-002/003 are addressed. Do not run them on untrusted manifests/models with sensitive host filesystem access.
- Do not pass real Nakama session tokens through the built-in HTTP bridge over non-loopback networks; it does not implement TLS.
- Dependency CVE status is unknown, not clean. Re-run dependency audit with network/cache access and record exact tooling/output.
- The secret scan was limited to current working-tree patterns and example files; perform a dedicated full-history scanner before making any “no secrets ever committed” claim.

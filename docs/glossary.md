# Glossary

| Category | Term | Definition | Reference |
|---|---|---|---|
| Product | Ahamkara | The reusable engine, runtime host, SDK, engine tools, and tests; currently the name of the transitional monorepo as well. | [Architecture](architecture/overview.md) |
| Product | Flashback | The game that consumes Ahamkara and optionally Wish; current code is mixed across `game/`, `client/`, `server/`, assets, and a thin sample launcher. | [Repository split](architecture/repository-split.md) |
| Product | Wish | An independent backend/session/activity platform with a game-neutral protocol and service integrations. | [Wish architecture](wish/architecture.md) |
| Architecture | Transitional monorepo | The current checkout containing engine, game, client, server, Wish, and Flashback code before extraction. | [Repository map](repo-map.md) |
| Architecture | Product boundary | The API, build, package, and ownership line that prevents lower-level repositories from importing product code. | [Repository split](architecture/repository-split.md) |
| Runtime | Game module | The proposed interface through which a game supplies simulation and presentation behavior to a generic Ahamkara host. | [Architecture](architecture/overview.md) |
| Operations | Error code | Stable product/domain/number identity for a handled failure, independent of its mutable message and incident occurrence. | [Error design](design/error-system.md) |
| Operations | Incident ID | Per-occurrence identifier used to correlate UI, logs, telemetry, client/server evidence, and diagnostics. | [Error design](design/error-system.md) |
| Operations | Diagnostic bundle | Timestamped system/config/log/crash evidence written by the core diagnostics facility. | [Debug operations](operations/debug-operations.md) |

## Abbreviations

| Abbreviation | Meaning |
|---|---|
| `AE` | Ahamkara error-code namespace |
| `FB` | Flashback error-code namespace |
| `WS` | Wish error-code namespace |
| `SDK` | Software development kit |
| `ABI` | Application binary interface |
| `PBR` | Physically based rendering |
| `LOD` | Level of detail |
| `IK` | Inverse kinematics |

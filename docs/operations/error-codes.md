# Ahamkara Error Codes (AE-*)

Stable, searchable identities for engine failures. Message text may change; codes do not.

## Active foundation codes

| Code | Title | Recovery | Owner |
|---|---|---|---|
| `AE-CFG-0001` | Configuration failure | Restart app | core/config |
| `AE-AST-0001` | Asset load failure | Retry (bounded) | core/assets |
| `AE-NET-0001` | Socket failure | Retry (bounded) | network |
| `AE-RND-0001` | Renderer init failure | Restart app | render |
| `AE-AUD-0001` | Audio init failure | Restart subsystem | audio |

## Player presentation

```
Code: AE-NET-0001
Incident: 7F4A-19C2
```

No paths, tokens, IPs, stack traces, or backend bodies are shown to players.

## API surface

- `ae/core/error_code.h` — `ErrorCode`
- `ae/core/error_types.h` — `Error`, `Result<T>`, `IncidentId`, `SmallContext`
- `ae/core/error_registry.h` — descriptor registry + active AE catalog
- `ae/core/error_report.h` — single reporting boundary

C-compatible view note: `ErrorCode::text()` is a stable ASCII string suitable for FFI as `const char*` via a temporary null-terminated buffer owned by the caller.

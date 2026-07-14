# Error-code operations and catalog

Status: Proposed catalog; no codes below are implemented yet

Activation work is tracked in
[Ahamkara foundation #62](https://github.com/tfq26/Project-Ahamkara/issues/62).
Wish and Flashback extensions are tracked independently in
[#63](https://github.com/tfq26/Project-Ahamkara/issues/63) and
[#64](https://github.com/tfq26/Project-Ahamkara/issues/64).

The code format and runtime model are specified in
[`../design/error-system.md`](../design/error-system.md). This document owns the
operator/support workflow and will become the per-code catalog as codes land.

## Player-facing contract

Every major handled failure should present:

- a short action in plain language;
- one stable code;
- one incident ID;
- a support/status route appropriate to that code.

Example:

```text
The connection to the session was lost.
Try reconnecting. If this continues, check service status.

Code: AE-NET-1004
Incident: 7F4A-19C2
```

The message may be localized or improved. The code retains the same meaning.

## Operator triage

1. Search telemetry and logs by code and time window.
2. Join the exact occurrence using the incident ID.
3. Check service/platform status before prescribing a local repair for a
   potentially remote failure.
4. Compare frequency by build, platform, subsystem, and recovery outcome.
5. Inspect the causal chain and bounded context in internal diagnostics.
6. If one code consistently contains several causes with different remedies,
   split future occurrences into new codes; do not redefine the old code.

This follows the useful part of Bungie's public process: code-specific support,
status checks, and pattern analysis across repeated codes.
[src: url: https://help.bungie.net/hc/en-us/articles/360049496971-Error-Codes-Disconnected-From-Destiny]

## Proposed Ahamkara catalog

| Code | Meaning | Player action | Owner |
|---|---|---|---|
| `AE-CFG-1001` | Configuration is invalid | Correct/reset configuration | `engine/core` |
| `AE-AST-1001` | Required asset is missing | Verify/install content | asset system |
| `AE-AST-1002` | Asset content is corrupt | Repair content | asset system |
| `AE-PLT-1001` | Window or platform surface creation failed | Restart/check platform support | `engine/platform` |
| `AE-RUN-1001` | Game module could not load | Install compatible module/build | `engine/runtime` |
| `AE-NET-1001` | Socket could not open | Check address, port, and permissions | `engine/network` |
| `AE-NET-1002` | Protocol versions are incompatible | Update client/server | `engine/network` |
| `AE-NET-1003` | Connection handshake timed out | Retry; then check status/network | `engine/network` |
| `AE-NET-1004` | Established connection was lost | Reconnect; then check status/network | `engine/network` |
| `AE-RND-1001` | No supported render backend is available | Update/check graphics support | `engine/render` |
| `AE-RND-1002` | Required shader failed to compile | Repair content/report defect | `engine/render` |
| `AE-RND-1003` | Render device/context was lost | Recreate or restart | `engine/render` |
| `AE-PHY-1001` | Physics backend initialization failed | Restart/report defect | `engine/physics` |
| `AE-AUD-1001` | Audio device initialization failed | Continue muted or select device | `engine/audio` |
| `AE-TOL-1001` | Asset compilation failed | Correct source asset | `tools` |

## Lifecycle

| State | Meaning |
|---|---|
| Reserved | Design/catalog entry exists; runtime must not emit it |
| Active | Descriptor, implementation, tests, support entry, and telemetry exist |
| Deprecated | New occurrences use a replacement; historical meaning is retained |
| Alias | Old transport/input identity maps to an active code without changing historical data |

Never delete or reuse an active/deprecated code. Add a replacement and document
the migration.

## Adding a code

The implementation change must include:

1. one unused domain number;
2. descriptor and owner;
3. safe player message key and support action;
4. native-error translation if applicable;
5. unit tests for identity/formatting;
6. recovery or fatal-boundary test;
7. logging/telemetry integration without high-cardinality labels;
8. catalog update here.

The corresponding implementation work is tracked in GitHub Issues. This file
records stable meanings, not completion state.

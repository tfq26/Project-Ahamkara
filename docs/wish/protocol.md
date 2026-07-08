# Wish Engine Protocol

## Message flow

1. Client sends `ClientHello` over UDP with:
   - packet envelope
   - `protocol_version`
   - reserved `session_token` field for future Nakama validation
2. Server validates the packet header and then checks `protocol_version`.
3. If the version matches, server replies with `ServerWelcome`.
4. If the version does not match, server replies with `ServerReject`.
5. After `ServerWelcome`, gameplay packets flow as before:
   - `PlayerInput`
   - `ServerSnapshot`

## Packet notes

- Packet headers still carry the shared wire-format version check.
- Handshake payloads also carry an explicit protocol version field so the server can reject unsupported clients before gameplay traffic starts.
- The handshake carries a `session_token`. When the Nakama bridge is enabled, the dedicated server validates that token against Nakama before session admission.
- Transport remains UDP-only.

## Compatibility notes

- Existing input/snapshot serialization is unchanged.
- Old clients that skip the handshake will no longer be accepted by the dedicated server.
- A version mismatch now fails fast with a reject packet instead of entering gameplay.

# Ahamkara Networking

## Model

Ahamkara uses a dedicated authoritative server model. Clients send input commands to the server,
the server simulates authoritative game state, and the server sends snapshots back to clients.

## Current Loop

The first implementation keeps the loop intentionally small:

- The client sends `PlayerInputCommand` packets over UDP at roughly 60 Hz.
- The server receives input, simulates simple movement, and updates one authoritative player state.
- The server sends `ServerSnapshot` packets back to the most recent client.
- The client prints snapshot tick and replicated player position.
- Packets now carry an explicit magic value, version, and packet kind so the wire format does not
  depend on compiler struct layout.

## Future Work

The current transport is a basic raw UDP loop. Future networking iterations are expected to add:

- client-side prediction
- server reconciliation
- snapshot interpolation
- lag compensation
- richer compatibility handling for future packet versions

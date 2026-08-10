# Ahamkara game MCP bridge

This is a development-only control path for autonomous gameplay tests. It is
split into two parts:

- `client/include/ahamkara/client/game_mcp_bridge.h` and its implementation are
  compiled only when `AHAMKARA_ENABLE_GAME_MCP=ON`.
- `tools/game_mcp_server.py` is an optional MCP SDK wrapper used by the desktop
  test agent.

The bridge is fail-closed. A game process must be built with the CMake option,
started with `AHAMKARA_GAME_MCP_ENABLED=1`, and given both
`AHAMKARA_GAME_MCP_ROOT` and `AHAMKARA_GAME_MCP_TOKEN`. Commands are atomic
files under `<root>/commands`; observations are atomically published as
`<root>/state.json`. The token is required on every command.

Production builds must leave `AHAMKARA_ENABLE_GAME_MCP` disabled. The CMake
configuration rejects the option for ordinary Release builds.

Example development setup:

```sh
export AHAMKARA_GAME_MCP_ENABLED=1
export AHAMKARA_GAME_MCP_ROOT="$PWD/build/game-mcp-session"
export AHAMKARA_GAME_MCP_TOKEN="replace-with-a-random-session-token"
export AHAMKARA_GAME_MCP_EXECUTABLE="$PWD/build/game-mcp/client/ahamkara_client"
export GAME_QA_MODEL="qwen/qwen3-vl-30b-a3b-instruct"
cmake -S . -B build/game-mcp -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DAHAMKARA_ENABLE_GAME_MCP=ON
cmake --build build/game-mcp --target ahamkara_client
python3 -m pip install -r tools/game_mcp_requirements.txt
python3 tools/game_mcp_server.py
```

The first slice exposes structured observations and bounded input actions. A
`game_capture_frame` adds authenticated framebuffer capture, and
`game_analyze_frame` sends one explicitly requested frame to the configured
OpenRouter vision model. A later slice will add deterministic scenario
reset/replay support.

"""Development-only MCP wrapper for the Ahamkara GameMcpBridge.

The wrapper is intentionally separate from the production game binary. It
uses the bridge's authenticated, atomic local-file protocol and refuses to
start unless the explicit development flag and token are present.
"""

from __future__ import annotations

import json
import os
import secrets
import subprocess
import time
from pathlib import Path
from typing import Any

from game_qa_model import VisionQaClient


class GameMcpServer:
    """Safe local controller for a dev build of Ahamkara."""

    def __init__(
        self,
        root: Path | None = None,
        token: str | None = None,
        *,
        enabled: bool | None = None,
    ) -> None:
        self.enabled = (
            enabled
            if enabled is not None
            else os.environ.get("AHAMKARA_GAME_MCP_ENABLED", "").lower() in {"1", "true", "yes"}
        )
        self.root = Path(root or os.environ.get("AHAMKARA_GAME_MCP_ROOT", ""))
        self.token = token or os.environ.get("AHAMKARA_GAME_MCP_TOKEN", "")
        if not self.enabled:
            raise RuntimeError("Game MCP is disabled; set AHAMKARA_GAME_MCP_ENABLED=1 for dev only")
        if not self.root or not self.token:
            raise RuntimeError("Game MCP requires AHAMKARA_GAME_MCP_ROOT and AHAMKARA_GAME_MCP_TOKEN")
        self.root = self.root.absolute()
        self.commands = self.root / "commands"
        self.state_path = self.root / "state.json"
        self.ready_path = self.root / "ready.json"
        self.process: subprocess.Popen[str] | None = None

    def observe(self) -> dict[str, Any]:
        if not self.state_path.exists():
            return {"ready": self.ready_path.exists(), "state": None}
        return json.loads(self.state_path.read_text(encoding="utf-8"))

    def send_action(self, *, duration_ticks: int = 1, **values: Any) -> dict[str, Any]:
        """Queue one bounded action and return the latest observation."""
        if duration_ticks < 1 or duration_ticks > 600:
            raise ValueError("duration_ticks must be between 1 and 600")
        self.commands.mkdir(parents=True, exist_ok=True)
        fields = {"token": self.token, "duration_ticks": duration_ticks}
        fields.update(values)
        payload = " ".join(
            f"{key}={str(value).lower() if isinstance(value, bool) else value}" for key, value in fields.items()
        )
        nonce = secrets.token_hex(8)
        temporary = self.commands / f"action-{time.time_ns()}-{nonce}.tmp"
        final = temporary.with_suffix(".cmd")
        temporary.write_text(payload + "\n", encoding="utf-8")
        temporary.replace(final)
        return self.observe()

    def step(
        self,
        *,
        move_x: float = 0.0,
        move_y: float = 0.0,
        look_x: float = 0.0,
        look_y: float = 0.0,
        duration_ticks: int = 1,
        **buttons: bool,
    ) -> dict[str, Any]:
        return self.send_action(
            move_x=move_x,
            move_y=move_y,
            look_x=look_x,
            look_y=look_y,
            duration_ticks=duration_ticks,
            **buttons,
        )

    def capture_frame(self, timeout_seconds: float = 5.0) -> dict[str, Any]:
        """Request a framebuffer capture and return its local path."""
        request = self.root / "capture.request"
        temporary = request.with_suffix(".request.tmp")
        temporary.write_text(self.token + "\n", encoding="utf-8")
        temporary.replace(request)
        request_mtime = request.stat().st_mtime_ns
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            frame = self.root / "frame.ppm"
            if frame.exists() and frame.stat().st_mtime_ns >= request_mtime:
                return {"path": str(frame), "mime_type": "image/x-portable-pixmap"}
            time.sleep(0.05)
        raise TimeoutError("The game did not produce a framebuffer capture")

    def launch(self) -> dict[str, Any]:
        """Launch only the explicitly configured dev executable."""
        executable = os.environ.get("AHAMKARA_GAME_MCP_EXECUTABLE", "")
        if not executable:
            raise RuntimeError("AHAMKARA_GAME_MCP_EXECUTABLE is not configured")
        path = Path(executable).absolute()
        allowed_root = self.root.parent.absolute()
        if path.parent != allowed_root and allowed_root not in path.parents:
            raise RuntimeError("Configured executable is outside the MCP development root")
        if self.process and self.process.poll() is None:
            return {"running": True, "pid": self.process.pid}
        self.process = subprocess.Popen(
            [str(path), "--local"],
            cwd=str(allowed_root),
            env=os.environ.copy(),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return {"running": True, "pid": self.process.pid}

    def stop(self) -> dict[str, Any]:
        if self.process and self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=5)
        return {"running": False}


def create_mcp_server() -> Any:
    """Build the optional MCP SDK server; importing this module stays stdlib-only."""
    from mcp.server.fastmcp import FastMCP

    bridge = GameMcpServer()
    server = FastMCP("ahamkara-game-dev")

    @server.tool()
    def game_observe() -> dict[str, Any]:
        """Return the latest structured game observation."""
        return bridge.observe()

    @server.tool()
    def game_step(
        move_x: float = 0.0,
        move_y: float = 0.0,
        look_x: float = 0.0,
        look_y: float = 0.0,
        duration_ticks: int = 1,
        fire: bool = False,
        aim: bool = False,
        reload: bool = False,
        jump: bool = False,
        sprint: bool = False,
        crouch: bool = False,
    ) -> dict[str, Any]:
        """Send one bounded gameplay action and return the current observation."""
        return bridge.step(
            move_x=move_x,
            move_y=move_y,
            look_x=look_x,
            look_y=look_y,
            duration_ticks=duration_ticks,
            fire=fire,
            aim=aim,
            reload=reload,
            jump=jump,
            sprint=sprint,
            crouch=crouch,
        )

    @server.tool()
    def game_launch() -> dict[str, Any]:
        """Launch the explicitly configured development game binary."""
        return bridge.launch()

    @server.tool()
    def game_stop() -> dict[str, Any]:
        """Stop the development game process started by this server."""
        return bridge.stop()

    @server.tool()
    def game_capture_frame(timeout_seconds: float = 5.0) -> dict[str, Any]:
        """Capture the current game framebuffer for visual QA."""
        return bridge.capture_frame(timeout_seconds)

    @server.tool()
    def game_analyze_frame(prompt: str, timeout_seconds: float = 5.0) -> dict[str, Any]:
        """Capture one frame and analyze it with the configured OpenRouter vision model."""
        capture = bridge.capture_frame(timeout_seconds)
        return VisionQaClient().analyze_frame(Path(capture["path"]), bridge.observe(), prompt)

    return server


if __name__ == "__main__":
    create_mcp_server().run()

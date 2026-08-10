import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from game_mcp_server import GameMcpServer
from game_qa_model import _ppm_to_png


class GameMcpServerTests(unittest.TestCase):
    def test_action_is_written_atomically_with_token(self):
        with tempfile.TemporaryDirectory() as directory:
            bridge = GameMcpServer(Path(directory), "test-token", enabled=True)
            bridge.step(move_y=1.0, fire=True, duration_ticks=3)
            files = list((Path(directory) / "commands").glob("*.cmd"))
            self.assertEqual(len(files), 1)
            contents = files[0].read_text(encoding="utf-8")
            self.assertIn("token=test-token", contents)
            self.assertIn("move_y=1.0", contents)
            self.assertIn("fire=true", contents)
            self.assertIn("duration_ticks=3", contents)

    def test_observe_returns_state_json(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "state.json").write_text(json.dumps({"frame": 7}), encoding="utf-8")
            bridge = GameMcpServer(root, "test-token", enabled=True)
            self.assertEqual(bridge.observe(), {"frame": 7})

    def test_disabled_server_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "disabled"):
            GameMcpServer(Path(tempfile.gettempdir()), "test-token", enabled=False)

    def test_frame_converter_returns_png(self):
        with tempfile.TemporaryDirectory() as directory:
            frame = Path(directory) / "frame.ppm"
            frame.write_bytes(b"P6\n1 1\n255\n" + bytes((10, 20, 30)))
            self.assertTrue(_ppm_to_png(frame).startswith(b"\x89PNG\r\n\x1a\n"))

    def test_invalid_launch_mode_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "ahamkara_client"
            executable.write_text("placeholder", encoding="utf-8")
            bridge = GameMcpServer(Path(directory), "test-token", enabled=True)
            with (
                patch.dict(
                    os.environ,
                    {
                        "AHAMKARA_GAME_MCP_EXECUTABLE": str(executable),
                        "AHAMKARA_GAME_MCP_LAUNCH_MODE": "menu",
                        "AHAMKARA_GAME_MCP_WORKING_DIRECTORY": directory,
                    },
                ),
                self.assertRaisesRegex(RuntimeError, "must be 'mcp' or 'local'"),
            ):
                bridge.launch()

    def test_invalid_working_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "ahamkara_client"
            executable.write_text("placeholder", encoding="utf-8")
            bridge = GameMcpServer(Path(directory), "test-token", enabled=True)
            with (
                patch.dict(
                    os.environ,
                    {
                        "AHAMKARA_GAME_MCP_EXECUTABLE": str(executable),
                        "AHAMKARA_GAME_MCP_WORKING_DIRECTORY": str(Path(directory) / "missing"),
                    },
                ),
                self.assertRaisesRegex(RuntimeError, "not a directory"),
            ):
                bridge.launch()


if __name__ == "__main__":
    unittest.main()

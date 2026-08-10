"""Optional OpenRouter vision client for explicit game QA requests."""

from __future__ import annotations

import base64
import binascii
import json
import os
import struct
import urllib.request
import zlib
from pathlib import Path
from typing import Any


def _ppm_to_png(path: Path) -> bytes:
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise ValueError("expected a binary PPM framebuffer")
    header_end = data.find(b"\n255\n")
    if header_end < 0:
        raise ValueError("invalid PPM header")
    header = data[:header_end].split()
    if len(header) != 3:
        raise ValueError("invalid PPM dimensions")
    width, height = int(header[1]), int(header[2])
    pixels = data[header_end + len(b"\n255\n") :]
    if len(pixels) != width * height * 3:
        raise ValueError("PPM pixel data has an unexpected size")

    def chunk(kind: bytes, payload: bytes) -> bytes:
        checksum = binascii.crc32(kind + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)

    scanlines = b"".join(b"\x00" + pixels[row * width * 3 : (row + 1) * width * 3] for row in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scanlines, level=6))
        + chunk(b"IEND", b"")
    )


class VisionQaClient:
    """Calls the configured OpenRouter vision model only on explicit request."""

    def __init__(self, api_key: str | None = None, model: str | None = None) -> None:
        self.api_key = api_key or os.environ.get("OPENROUTER_API_KEY", "")
        self.model = model or os.environ.get("GAME_QA_MODEL", "qwen/qwen3-vl-30b-a3b-instruct")
        self.base_url = os.environ.get("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1")
        if not self.api_key:
            raise RuntimeError("OPENROUTER_API_KEY is not set for game QA")

    def analyze_frame(self, image_path: Path, observation: dict[str, Any], prompt: str) -> dict[str, Any]:
        image = base64.b64encode(_ppm_to_png(image_path)).decode("ascii")
        request_body = {
            "model": self.model,
            "messages": [
                {
                    "role": "system",
                    "content": (
                        "You are a game QA analyst. Inspect the screenshot and structured state. "
                        "Return JSON with verdict, findings, severity, and recommended_next_step. "
                        "Do not claim a defect without visual or state evidence."
                    ),
                },
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": json.dumps({"task": prompt, "state": observation})},
                        {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{image}"}},
                    ],
                },
            ],
            "max_tokens": 1200,
            "temperature": 0,
            "response_format": {"type": "json_object"},
        }
        request = urllib.request.Request(
            f"{self.base_url.rstrip('/')}/chat/completions",
            data=json.dumps(request_body).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json",
                "X-OpenRouter-Title": "Ahamkara Game QA",
            },
        )
        with urllib.request.urlopen(request, timeout=120) as response:
            payload = json.load(response)
        content = payload["choices"][0]["message"]["content"]
        result = json.loads(content)
        if not isinstance(result, dict):
            raise RuntimeError("Game QA model did not return a JSON object")
        return result

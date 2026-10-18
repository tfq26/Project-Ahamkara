#!/usr/bin/env python3
"""Regression contract for the authored first-person AR viewmodel."""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    gltf_path = repo / "assets/models/viewmodel_ar15.gltf"
    document = json.loads(gltf_path.read_text())
    accessor = document["accessors"][0]
    view = document["bufferViews"][accessor["bufferView"]]
    buffer = (gltf_path.parent / document["buffers"][0]["uri"]).read_bytes()
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 12)
    positions = [struct.unpack_from("<3f", buffer, offset + index * stride)
                 for index in range(accessor["count"])]
    minimum = tuple(min(point[axis] for point in positions) for axis in range(3))
    maximum = tuple(max(point[axis] for point in positions) for axis in range(3))
    span = tuple(maximum[axis] - minimum[axis] for axis in range(3))

    # A first-person rifle is long on canonical X (barrel direction), not Y.
    if span[0] < 1.2 or span[1] > 0.7 or span[2] > 0.7:
        print(f"unexpected AR viewmodel bounds: min={minimum}, max={maximum}, span={span}")
        return 1
    print(f"AR viewmodel axis contract OK: span={span}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

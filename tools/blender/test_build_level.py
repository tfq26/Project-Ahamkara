#!/usr/bin/env python3
"""Pure (bpy-free) unit tests for tools/blender/build_level.py.

Runnable without Blender:  python3 tools/blender/test_build_level.py
Exits 0 on success, 1 on failure.
"""

import math
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_level  # noqa: E402
import spec_to_lvl  # noqa: E402  (placed on path by build_level import)


def _close(a: float, b: float) -> bool:
    return math.isclose(a, b, rel_tol=1e-6, abs_tol=1e-6)


def fail(msg: str) -> int:
    print(f"test_build_level FAIL: {msg}", file=sys.stderr)
    return 1


def test_plan_blockout() -> int:
    spec = {
        "collision": [
            {"min": [-2.0, -4.0], "max": [2.0, 4.0], "top_y": 3.0, "bottom_y": 1.0, "wall": True},
        ],
        "meshes": [
            {"mesh": "a.aemesh", "pos": [1.0, 2.0, 3.0]},
            {"mesh": "b.aemesh", "pos": [0.0, 0.0, 0.0]},
        ],
        "spawns": [{"pos": [5.0, 1.0, 5.0], "yaw": 90.0}],
    }
    plan = build_level.plan_blockout(spec)

    if len(plan["boxes"]) != 1:
        return fail("expected 1 box")
    box = plan["boxes"][0]
    if not all(_close(a, b) for a, b in zip(box["center"], [0.0, 2.0, 0.0])):
        return fail(f"box center wrong: {box['center']}")
    if not all(_close(a, b) for a, b in zip(box["size"], [4.0, 2.0, 8.0])):
        return fail(f"box size wrong: {box['size']}")
    if not box["wall"]:
        return fail("box wall flag lost")

    if len(plan["instances"]) != 2:
        return fail("expected 2 instances")
    if plan["instances"][0]["mesh"] != "a.aemesh":
        return fail("instance mesh id wrong")
    if len(plan["spawns"]) != 1 or not _close(plan["spawns"][0]["yaw"], 90.0):
        return fail("spawn passthrough wrong")
    return 0


def test_shared_lvl_writer() -> int:
    # build_level must emit the exact same .lvl as the Path A writer.
    spec = {
        "name": "Shared Writer Check",
        "gravity": 12.5,
        "spawns": [{"pos": [0.0, 1.0, 0.0], "yaw": 0.0, "team": 1}],
        "collision": [{"min": [-5.0, -5.0], "max": [5.0, 5.0], "top_y": 0.0, "bottom_y": -0.5}],
        "meshes": [{"mesh": "x.aemesh", "pos": [1.0, 1.0, 1.0], "scale": [2.0, 2.0, 2.0]}],
    }
    expected = spec_to_lvl.spec_to_lvl_text(spec, "spec.json")
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "out.lvl"
        build_level.write_lvl(spec, out, "spec.json")
        got = out.read_text()
    if got != expected:
        return fail("build_level .lvl output differs from Path A emitter")
    return 0


def test_real_prototype_spec() -> int:
    spec_path = Path(__file__).resolve().parents[2] / "assets" / "levels" / "prototype_box.json"
    if not spec_path.exists():
        return fail(f"missing prototype spec: {spec_path}")
    spec = build_level.load_spec(str(spec_path))
    plan = build_level.plan_blockout(spec)
    if len(plan["instances"]) != 2:
        return fail("prototype_box should yield 2 mesh instances")
    if build_level.output_basename(spec) != "prototype_box_showcase":
        return fail(f"unexpected basename: {build_level.output_basename(spec)}")
    return 0


def main() -> int:
    for test in (test_plan_blockout, test_shared_lvl_writer, test_real_prototype_spec):
        rc = test()
        if rc != 0:
            return rc
    print("test_build_level passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

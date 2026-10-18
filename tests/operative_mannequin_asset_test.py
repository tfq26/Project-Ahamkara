#!/usr/bin/env python3
"""Validate the modular mannequin source contract."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET = ROOT / "assets/models/operative_mannequin.gltf"


def main() -> int:
    model = json.loads(ASSET.read_text())
    assert len(model.get("skins", [])) == 1
    joints = model["skins"][0]["joints"]
    assert 15 <= len(joints) <= 32
    animations = {animation["name"]: animation for animation in model.get("animations", [])}
    required = {
        "profile_stance", "crouch_idle", "jump_takeoff", "airborne_loop",
        "float_loop", "land_soft", "locomotion_walk", "dance_placeholder",
    }
    assert required <= animations.keys()
    assert all(len(animations[name]["channels"]) >= len(joints) * 3 for name in required)
    primitives = [primitive for mesh in model["meshes"] for primitive in mesh["primitives"]]
    assert primitives
    assert all("JOINTS_0" in primitive["attributes"] for primitive in primitives)
    assert all("WEIGHTS_0" in primitive["attributes"] for primitive in primitives)
    material_names = {material["name"] for material in model.get("materials", [])}
    assert {"Mannequin Shell", "Joint Polymer", "Skin Material Region"} <= material_names
    mesh_names = {mesh["name"] for mesh in model["meshes"]}
    assert {"Chest shell", "Pelvis shell", "Head base"} <= mesh_names
    assert (ROOT / "assets/models/operative_mannequin.blend").is_file()
    assert (ROOT / "assets/compiled/models/operative_mannequin.aemesh").is_file()
    assert (ROOT / "tools/blender/build_operative_mannequin.py").is_file()
    print("operative_mannequin_asset_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Validate the MPFB operative asset used by Character View and debug 3P."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    model_path = ROOT / "assets/models/operative_human_base.gltf"
    model = json.loads(model_path.read_text())

    assert len(model.get("meshes", [])) == 1
    assert len(model.get("skins", [])) == 1
    assert len(model["skins"][0]["joints"]) == 53
    animation_names = {animation.get("name") for animation in model.get("animations", [])}
    assert {"profile_stance", "dance_charleston"} <= animation_names
    profile = next(animation for animation in model["animations"] if animation["name"] == "profile_stance")
    assert len(profile.get("channels", [])) >= 50
    dance = next(animation for animation in model["animations"] if animation["name"] == "dance_charleston")
    assert len(dance.get("channels", [])) >= 60

    primitive = model["meshes"][0]["primitives"][0]
    assert "JOINTS_0" in primitive["attributes"]
    assert "WEIGHTS_0" in primitive["attributes"]
    assert (ROOT / "assets/models/operative_human_base.blend").is_file()
    assert (ROOT / "tools/blender/export_operative_rifle_idle_profile.py").is_file()
    finger_rig_script = ROOT / "tools/blender/add_operative_finger_rig.py"
    assert finger_rig_script.is_file()
    assert "validate_finger_weights" in finger_rig_script.read_text()
    nodes = model.get("nodes", [])
    skin_joint_names = {nodes[index]["name"] for index in model["skins"][0]["joints"]}
    for side in ("l", "r"):
        for finger in ("index", "middle", "ring", "pinky"):
            assert f"{finger}_01_{side}" in skin_joint_names
            assert f"{finger}_02_{side}" in skin_joint_names
            assert f"{finger}_03_{side}" in skin_joint_names
    source_animation = ROOT / "assets/source_animations/rifle_idle_mixamo.fbx"
    assert source_animation.is_file()
    assert (ROOT / "assets/compiled/models/operative_human_base.aemesh").is_file()
    profile_path = ROOT / "assets/models/operative_human_profile_static.gltf"
    profile = json.loads(profile_path.read_text())
    assert len(profile.get("meshes", [])) == 1
    assert not profile.get("skins")
    assert not profile.get("animations")
    assert (ROOT / "assets/compiled/models/operative_human_profile_static.aemesh").is_file()
    rifle_profile_path = ROOT / "assets/models/operative_human_rifle_idle_static.gltf"
    rifle_profile = json.loads(rifle_profile_path.read_text())
    assert len(rifle_profile.get("meshes", [])) == 2
    assert not rifle_profile.get("skins")
    assert not rifle_profile.get("animations")
    assert (ROOT / "assets/compiled/models/operative_human_rifle_idle_static.aemesh").is_file()

    script = ROOT / "tools/blender/retarget_operative_human_emotes.py"
    script_text = script.read_text()
    assert "BONE_MAP" in script_text
    assert "dance_charleston" in script_text
    profile_script = ROOT / "tools/blender/export_operative_profile_static.py"
    assert profile_script.is_file()
    assert "new_from_object(evaluated" in profile_script.read_text()
    rifle_profile_script = ROOT / "tools/blender/export_operative_rifle_idle_profile.py"
    rifle_profile_text = rifle_profile_script.read_text()
    assert "CARBINE_ASSET" in rifle_profile_text
    assert "pose_rifle_ready" in rifle_profile_text
    assert "SHOULDER_CONTACT" in rifle_profile_text
    assert "weapon_placement" in rifle_profile_text
    assert "Matrix.Translation(-STOCK_SHOULDER)" in rifle_profile_text
    print("operative_human_base_asset_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

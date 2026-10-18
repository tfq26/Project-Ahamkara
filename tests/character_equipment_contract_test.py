#!/usr/bin/env python3
"""Validate the canonical rig and third-person weapon attachment contracts."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    rig = json.loads((ROOT / "assets/characters/operative_human.rig.json").read_text())
    assert rig["retarget_root"] == "pelvis"
    assert rig["equipment_attachments"] == {
        "dominant_hand": "hand_r", "support_hand": "hand_l"}
    assert set(rig["chains"]) == {"spine", "left_arm", "right_arm", "left_leg", "right_leg"}
    assert set(rig["finger_chains"]) == {"left", "right"}
    for side in ("left", "right"):
        assert set(rig["finger_chains"][side]) == {"index", "middle", "ring", "pinky"}
        assert all(len(chain) == 3 for chain in rig["finger_chains"][side].values())

    sockets = json.loads((ROOT / "assets/equipment/operative_carbine_tp.sockets.json").read_text())
    assert sockets["coordinate_space"] == "weapon_local_meters"
    assert {"weapon_root", "right_hand_grip", "left_hand_grip", "stock_shoulder", "muzzle"} <= set(sockets["sockets"])
    assert sockets["sockets"]["right_hand_grip"] == [0.0, 0.0, 0.0]
    assert sockets["sockets"]["left_hand_grip"] == [0.255, 0.0, 0.075]
    assert sockets["sockets"]["stock_shoulder"] == [-0.245, 0.0, 0.055]

    builder = (ROOT / "tools/blender/build_operative_carbine.py").read_text()
    assert "right_hand_grip" in builder
    assert "left_hand_grip" in builder
    profile_exporter = (ROOT / "tools/blender/export_operative_rifle_idle_profile.py").read_text()
    assert "CARBINE_ASSET" in profile_exporter
    assert "operative_carbine_tp" in profile_exporter
    assert "pose_rifle_ready" in profile_exporter
    assert "constraint = bone.constraints.new(\"IK\")" in profile_exporter
    assert "weapon_placement" in profile_exporter
    assert "SHOULDER_CONTACT" in profile_exporter
    assert "weapon_placement" in profile_exporter
    assert "SHOULDER_CONTACT" in profile_exporter
    assert "Matrix.Translation(-STOCK_SHOULDER)" in profile_exporter
    assert "RIFLE_GRIP_PROFILE" in profile_exporter
    assert "author_rifle_grip(target)" in profile_exporter
    assert "finger_curls" in profile_exporter

    rifle_animation = json.loads(
        (ROOT / "assets/characters/operative_human_rifle.animation.json").read_text())
    assert rifle_animation["family"] == "rifle"
    assert rifle_animation["attachments"] == {
        "dominant_hand": "hand_r",
        "support_hand": "hand_l",
        "dominant_grip_socket": "right_hand_grip",
        "support_grip_socket": "left_hand_grip",
        "shoulder_socket": "stock_shoulder",
    }
    assert rifle_animation["layers"] == [
        "locomotion", "rifle_upper_body", "aim_offset", "support_hand_ik", "recoil"]
    assert set(rifle_animation["clips"]) == {"idle", "aim", "fire", "reload"}
    print("character_equipment_contract_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

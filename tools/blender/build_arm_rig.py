#!/usr/bin/env python3
"""Build a skinned first-person arm rig with animations and export to glTF.

Usage:
    blender -b -P tools/blender/build_arm_rig.py -- --out_dir assets/models
    blender    -P tools/blender/build_arm_rig.py -- --out_dir assets/models --open

Produces viewmodel_arms.gltf (with skins + 3 animations):
  - idle: 2s loop, subtle breathing sway
  - fire: 0.3s, recoil kick backward
  - reload: 1.5s, magazine swap
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

try:
    import bpy
    from mathutils import Euler, Matrix, Quaternion, Vector
except ImportError:
    raise RuntimeError("build_arm_rig.py must be run inside Blender")

# ── Constants ─────────────────────────────────────────────────────────────────

FPS = 60
ARM_LENGTHS = {
    "upper": 0.35,
    "forearm": 0.28,
    "hand": 0.12,
}

JOINT_POSITIONS = {
    "root": (0.0, 0.0, 0.0),
    "shoulder": (0.0, 0.0, 0.0),
    "elbow": (0.0, -ARM_LENGTHS["upper"], 0.0),
    "wrist": (0.0, -ARM_LENGTHS["upper"] - ARM_LENGTHS["forearm"], 0.0),
    "hand": (0.0, -ARM_LENGTHS["upper"] - ARM_LENGTHS["forearm"] - ARM_LENGTHS["hand"] * 0.5, 0.0),
    "weapon_attach": (0.0, -ARM_LENGTHS["upper"] - ARM_LENGTHS["forearm"] - ARM_LENGTHS["hand"], 0.0),
}

BONE_CHAIN = ["root", "shoulder", "elbow", "wrist", "hand", "weapon_attach"]
BONE_PARENTS = {"shoulder": "root", "elbow": "shoulder", "wrist": "elbow", "hand": "wrist", "weapon_attach": "hand"}

# ── Build ─────────────────────────────────────────────────────────────────────


def build_arm_mesh() -> tuple:
    """Create a simple skinned arm mesh (two cylinders)."""
    # Create armature
    bpy.ops.object.armature_add(enter_editmode=False, location=(0, 0, 0))
    armature = bpy.context.active_object
    armature.name = "Armature"
    armature.data.name = "ArmatureData"

    # Enter edit mode to build bones
    bpy.ops.object.mode_set(mode="EDIT")
    edit_bones = armature.data.edit_bones

    bones = {}
    for name in BONE_CHAIN:
        bone = edit_bones.new(name)
        pos = JOINT_POSITIONS[name]
        bone.head = Vector(pos)
        bone.tail = Vector(pos) + Vector((0, -0.05, 0))  # tiny default length
        bones[name] = bone

    # Set parents and adjust tails to point to child
    for name in BONE_CHAIN[1:]:
        parent_name = BONE_PARENTS[name]
        bones[name].parent = bones[parent_name]
        # Extend tail toward next joint (or downward if last)
        child_idx = BONE_CHAIN.index(name) + 1
        if child_idx < len(BONE_CHAIN):
            child_pos = JOINT_POSITIONS[BONE_CHAIN[child_idx]]
            bones[name].tail = Vector(child_pos)
        else:
            bones[name].tail = Vector(JOINT_POSITIONS[name]) + Vector((0, -0.08, 0))

    # Fix root: head at shoulder, tail at shoulder (zero-length until child)
    bones["root"].head = Vector(JOINT_POSITIONS["root"])
    bones["root"].tail = Vector(JOINT_POSITIONS["shoulder"]) + Vector((0, -0.01, 0))

    bpy.ops.object.mode_set(mode="OBJECT")

    # Create mesh (two cylinders: upper arm + forearm)
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=12, radius=0.04, depth=ARM_LENGTHS["upper"], location=(0, -ARM_LENGTHS["upper"] * 0.5, 0)
    )
    upper = bpy.context.active_object
    upper.name = "UpperArm"

    bpy.ops.mesh.primitive_cylinder_add(
        vertices=10,
        radius=0.035,
        depth=ARM_LENGTHS["forearm"],
        location=(0, -ARM_LENGTHS["upper"] - ARM_LENGTHS["forearm"] * 0.5, 0),
    )
    forearm = bpy.context.active_object
    forearm.name = "Forearm"

    bpy.ops.mesh.primitive_cylinder_add(
        vertices=8,
        radius=0.03,
        depth=ARM_LENGTHS["hand"] * 0.5,
        location=(0, -ARM_LENGTHS["upper"] - ARM_LENGTHS["forearm"] - ARM_LENGTHS["hand"] * 0.25, 0),
    )
    hand = bpy.context.active_object
    hand.name = "Hand"

    # Join mesh parts
    bpy.ops.object.select_all(action="DESELECT")
    upper.select_set(True)
    forearm.select_set(True)
    hand.select_set(True)
    bpy.context.view_layer.objects.active = upper
    bpy.ops.object.join()
    mesh_obj = bpy.context.active_object
    mesh_obj.name = "ArmMesh"

    # Parent mesh to armature with automatic weights
    mesh_obj.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")

    # Apply material
    mat = bpy.data.materials.new("ArmMat")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.85, 0.78, 0.68, 1.0)
        bsdf.inputs["Metallic"].default_value = 0.0
        bsdf.inputs["Roughness"].default_value = 0.6
    mesh_obj.data.materials.append(mat)

    return armature, mesh_obj


# ── Animation helpers ─────────────────────────────────────────────────────────


def key_bone(armature, bone_name, frame, rotation_euler=None, rotation_quat=None, location=None):
    """Insert a keyframe for a bone at the given frame."""
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="POSE")

    pose_bone = armature.pose.bones.get(bone_name)
    if pose_bone is None:
        bpy.ops.object.mode_set(mode="OBJECT")
        return

    if rotation_euler is not None:
        pose_bone.rotation_mode = "XYZ"
        pose_bone.rotation_euler = Euler(rotation_euler, "XYZ")
    if rotation_quat is not None:
        pose_bone.rotation_mode = "QUATERNION"
        pose_bone.rotation_quat = Quaternion(rotation_quat)
    if location is not None:
        pose_bone.location = Vector(location)

    pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
    bpy.ops.object.mode_set(mode="OBJECT")


def make_animation(armature, name, keyframes_func):
    """Create a new action, call keyframes_func(frame), store it."""
    # Create fresh action
    action = bpy.data.actions.new(name)
    armature.animation_data.action = action

    scene = bpy.context.scene
    scene.frame_start = 0
    scene.frame_end = 0  # will expand

    def key(bone_name, frame, rotation_euler=None):
        pose_bone = armature.pose.bones.get(bone_name)
        if pose_bone is None:
            return
        if rotation_euler is not None:
            pose_bone.rotation_mode = "QUATERNION"
            pose_bone.rotation_quaternion = Euler(rotation_euler, "XYZ").to_quaternion()
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
        if frame > scene.frame_end:
            scene.frame_end = frame

    keyframes_func(key)

    action.use_fake_user = True
    # Push to NLA
    track = armature.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, 0, action)
    strip.name = name


def make_idle(armature):
    def keyframes(key):
        for f in range(0, int(FPS * 2) + 1, 3):
            t = f / float(FPS)
            shoulder_x = math.sin(t * 1.7) * 0.04
            elbow_x = math.cos(t * 2.1) * 0.015
            wrist_z = math.sin(t * 2.8) * 0.02
            key("shoulder", f, rotation_euler=(0, shoulder_x, 0))
            key("elbow", f, rotation_euler=(elbow_x, 0, 0))
            key("wrist", f, rotation_euler=(0, 0, wrist_z))

    make_animation(armature, "idle", keyframes)


def make_fire(armature):
    def keyframes(key):
        dur = int(FPS * 0.3)
        key("shoulder", 0, (0, 0, 0))
        key("elbow", 0, (0, 0, 0))
        key("wrist", 0, (0, 0, 0))
        key("shoulder", 2, (0, 0, -0.08))
        key("elbow", 2, (0.12, 0, 0))
        key("wrist", 2, (-0.25, 0, 0))
        key("shoulder", 4, (0, 0, 0.02))
        key("elbow", 4, (-0.03, 0, 0))
        key("wrist", 4, (0.05, 0, 0))
        key("shoulder", dur, (0, 0, 0))
        key("elbow", dur, (0, 0, 0))
        key("wrist", dur, (0, 0, 0))

    make_animation(armature, "fire", keyframes)


def make_reload(armature):
    def keyframes(key):
        dur = int(FPS * 1.5)
        t1 = int(FPS * 0.4)
        t2 = int(FPS * 0.8)
        t3 = int(FPS * 1.2)
        key("shoulder", 0, (0, 0, 0))
        key("elbow", 0, (0, 0, 0))
        key("wrist", 0, (0, 0, 0))
        key("shoulder", t1, (0, 0, -0.5))
        key("elbow", t1, (0.3, 0, 0))
        key("wrist", t1, (0, 0, 1.2))
        key("shoulder", t2, (0, 0, -0.55))
        key("elbow", t2, (0.35, 0, 0))
        key("wrist", t2, (0, 0, 1.6))
        key("shoulder", t3, (0, 0, -0.15))
        key("elbow", t3, (0.05, 0, 0))
        key("wrist", t3, (0, 0, 0.2))
        key("shoulder", dur, (0, 0, 0))
        key("elbow", dur, (0, 0, 0))
        key("wrist", dur, (0, 0, 0))

    make_animation(armature, "reload", keyframes)


# ── Export ────────────────────────────────────────────────────────────────────


def export(out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    basename = "viewmodel_arms"

    # Build
    bpy.ops.wm.read_factory_settings(use_empty=True)
    armature, mesh = build_arm_mesh()
    if not armature.animation_data:
        armature.animation_data_create()
    make_idle(armature)
    make_fire(armature)
    make_reload(armature)

    # Push each action into its own NLA track with a strip
    if armature.animation_data:
        for action in bpy.data.actions:
            if action.name in ("idle", "fire", "reload"):
                track = armature.animation_data.nla_tracks.new()
                track.name = action.name
                strip = track.strips.new(action.name, 0, action)
                strip.name = action.name

    blend_path = out_dir / f"{basename}.blend"
    gltf_path = out_dir / f"{basename}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=True,
        export_nla_strips=True,
        export_materials="EXPORT",
    )

    action_count = len(bpy.data.actions)
    print(f"build_arm_rig: Actions saved: {[a.name for a in bpy.data.actions]}")
    print(f"build_arm_rig: NLA tracks: {len(armature.animation_data.nla_tracks)}")
    print(f"build_arm_rig: Exported {basename}.blend and {basename}.gltf ({action_count} animations)")
    return 0


def _parse_args(argv):
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("--out_dir", default="assets/models")
    p.add_argument("--open", action="store_true")
    if "--" in argv:
        return p.parse_args(argv[argv.index("--") + 1 :])
    return p.parse_args(argv[1:])


def main(argv):
    args = _parse_args(argv)
    out_dir = Path(args.out_dir).resolve()
    result = export(out_dir)
    if args.open and not bpy.app.background:
        bpy.ops.wm.open_mainfile(filepath=str(out_dir / "viewmodel_arms.blend"))
    return result


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

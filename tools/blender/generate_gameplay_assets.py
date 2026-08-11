#!/usr/bin/env python3
"""Generate the first untextured gameplay weapon and enemy assets.

Run inside Blender 4.4+:
    blender -b -P tools/blender/generate_gameplay_assets.py -- \
        --out_dir assets/models

The generated glTF files intentionally use simple grey materials. They are
playable rig/animation references, not final art.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

try:
    import bpy
    from mathutils import Euler, Vector
except ImportError as exc:  # pragma: no cover - Blender only
    raise RuntimeError("This script must run inside Blender") from exc


FPS = 30


def material(name: str, color: tuple[float, float, float, float]):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.78
        bsdf.inputs["Metallic"].default_value = 0.05
    return mat


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_armature(name: str, bones: dict[str, tuple[tuple[float, float, float], tuple[float, float, float], str | None]]):
    bpy.ops.object.armature_add(enter_editmode=True, location=(0.0, 0.0, 0.0))
    armature = bpy.context.object
    armature.name = name
    armature.data.name = f"{name}_Skeleton"
    edit_bones = armature.data.edit_bones
    edit_bones.remove(edit_bones[0])
    created = {}
    for bone_name, (head, tail, parent_name) in bones.items():
        bone = edit_bones.new(bone_name)
        bone.head = Vector(head)
        bone.tail = Vector(tail)
        if parent_name:
            bone.parent = created[parent_name]
        created[bone_name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    armature.show_in_front = True
    return armature


def bind_object(obj, armature, bone_name: str) -> None:
    obj.parent = armature
    modifier = obj.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    group = obj.vertex_groups.new(name=bone_name)
    group.add([vertex.index for vertex in obj.data.vertices], 1.0, "REPLACE")


def add_cube(name, location, scale, mat, armature, bone_name, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bevel_mod = obj.modifiers.new("Soft edges", "BEVEL")
        bevel_mod.width = bevel
        bevel_mod.segments = 2
    obj.data.materials.append(mat)
    bind_object(obj, armature, bone_name)
    return obj


def add_cylinder_between(name, start, end, radius, mat, armature, bone_name, vertices=12):
    start_v = Vector(start)
    end_v = Vector(end)
    direction = end_v - start_v
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=direction.length,
        location=(start_v + end_v) * 0.5,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    obj.data.materials.append(mat)
    bind_object(obj, armature, bone_name)
    return obj


def add_sphere(name, location, scale, mat, armature, bone_name):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    bind_object(obj, armature, bone_name)
    return obj


def pose_bone(armature, bone_name, frame, rotation=(0.0, 0.0, 0.0), location=(0.0, 0.0, 0.0)):
    bone = armature.pose.bones.get(bone_name)
    if bone is None:
        return
    bone.rotation_mode = "XYZ"
    bone.rotation_euler = Euler(rotation, "XYZ")
    bone.location = Vector(location)
    bone.keyframe_insert(data_path="rotation_euler", frame=frame)
    bone.keyframe_insert(data_path="location", frame=frame)


def make_action(armature, name, end_frame, poses) -> None:
    armature.animation_data_create()
    action = bpy.data.actions.new(name)
    armature.animation_data.action = action
    bpy.context.scene.frame_start = 0
    bpy.context.scene.frame_end = end_frame
    poses()
    armature.animation_data.action = None
    track = armature.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, 0, action)
    strip.action_frame_start = 0
    strip.action_frame_end = end_frame
    strip.frame_start = 0
    strip.frame_end = end_frame


def export_asset(armature, name: str, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    blend_path = out_dir / f"{name}.blend"
    gltf_path = out_dir / f"{name}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=True,
        export_nla_strips=True,
        export_skins=True,
        export_materials="EXPORT",
    )
    print(f"generated {blend_path} and {gltf_path}")


def build_assault_rifle(out_dir: Path) -> None:
    reset_scene()
    dark = material("Rifle Grey", (0.16, 0.18, 0.20, 1.0))
    black = material("Rifle Dark", (0.055, 0.065, 0.075, 1.0))
    bones = {
        "root": ((0, 0, 0), (0.15, 0, 0), None),
        "magazine": ((0.05, -0.12, 0), (0.05, -0.30, 0), "root"),
        "bolt": ((0.38, 0.06, 0), (0.52, 0.06, 0), "root"),
    }
    armature = make_armature("AssaultRifle_Rig", bones)
    add_cube("Receiver", (0.0, 0.0, 0.0), (0.52, 0.16, 0.16), dark, armature, "root", 0.025)
    add_cube("Handguard", (0.48, 0.0, 0.0), (0.62, 0.13, 0.13), black, armature, "root", 0.018)
    add_cylinder_between("Barrel", (0.78, 0.0, 0.0), (1.45, 0.0, 0.0), 0.035, dark, armature, "root", 16)
    add_cylinder_between("Muzzle", (1.42, 0.0, 0.0), (1.55, 0.0, 0.0), 0.06, black, armature, "root", 16)
    add_cube("Stock", (-0.42, 0.02, 0.0), (0.38, 0.13, 0.14), black, armature, "root", 0.02)
    add_cube("Grip", (-0.05, -0.18, 0.0), (0.12, 0.32, 0.12), black, armature, "root", 0.02)
    add_cube("Magazine", (0.08, -0.22, 0.0), (0.16, 0.34, 0.12), dark, armature, "magazine", 0.018)
    add_cube("Optic", (0.20, 0.13, 0.0), (0.24, 0.08, 0.08), dark, armature, "root", 0.012)
    add_cube("Bolt", (0.38, 0.07, 0.0), (0.18, 0.035, 0.035), black, armature, "bolt", 0.006)

    def idle():
        pose_bone(armature, "root", 0)
        pose_bone(armature, "root", 30)

    def reload():
        pose_bone(armature, "root", 0)
        pose_bone(armature, "magazine", 0)
        pose_bone(armature, "bolt", 0)
        pose_bone(armature, "root", 12, location=(0.0, -0.045, 0.025), rotation=(0.0, 0.0, math.radians(-3)))
        pose_bone(armature, "magazine", 12, location=(0.0, -0.16, 0.0))
        pose_bone(armature, "bolt", 12, location=(-0.12, 0.0, 0.0))
        pose_bone(armature, "root", 24, location=(0.0, -0.07, 0.035), rotation=(0.0, 0.0, math.radians(-5)))
        pose_bone(armature, "magazine", 24, location=(0.0, -0.16, 0.0))
        pose_bone(armature, "bolt", 24, location=(-0.12, 0.0, 0.0))
        pose_bone(armature, "root", 40)
        pose_bone(armature, "magazine", 40)
        pose_bone(armature, "bolt", 40)
        pose_bone(armature, "root", 54)
        pose_bone(armature, "magazine", 54)
        pose_bone(armature, "bolt", 54)

    def ads_in():
        pose_bone(armature, "root", 0)
        pose_bone(armature, "root", 12, location=(0.0, -0.025, -0.12), rotation=(0.0, math.radians(1.5), 0.0))
        pose_bone(armature, "root", 18, location=(0.0, -0.025, -0.12), rotation=(0.0, math.radians(1.5), 0.0))

    def ads_out():
        pose_bone(armature, "root", 0, location=(0.0, -0.025, -0.12), rotation=(0.0, math.radians(1.5), 0.0))
        pose_bone(armature, "root", 12)
        pose_bone(armature, "root", 18)

    make_action(armature, "idle", 30, idle)
    make_action(armature, "reload", 54, reload)
    make_action(armature, "ads_in", 18, ads_in)
    make_action(armature, "ads_out", 18, ads_out)
    export_asset(armature, "assault_rifle", out_dir)


def build_enemy(out_dir: Path) -> None:
    reset_scene()
    grey = material("Enemy Grey", (0.38, 0.40, 0.43, 1.0))
    dark = material("Enemy Dark", (0.18, 0.20, 0.23, 1.0))
    bones = {
        "root": ((0, 0, 0), (0, 0.25, 0), None),
        "pelvis": ((0, 0.95, 0), (0, 1.18, 0), "root"),
        "spine": ((0, 1.18, 0), (0, 1.55, 0), "pelvis"),
        "chest": ((0, 1.55, 0), (0, 1.88, 0), "spine"),
        "neck": ((0, 1.88, 0), (0, 2.05, 0), "chest"),
        "head": ((0, 2.05, 0), (0, 2.30, 0), "neck"),
        "upper_arm.L": ((-0.25, 1.78, 0), (-0.58, 1.50, 0), "chest"),
        "forearm.L": ((-0.58, 1.50, 0), (-0.78, 1.23, 0), "upper_arm.L"),
        "hand.L": ((-0.78, 1.23, 0), (-0.84, 1.14, 0), "forearm.L"),
        "upper_arm.R": ((0.25, 1.78, 0), (0.58, 1.50, 0), "chest"),
        "forearm.R": ((0.58, 1.50, 0), (0.78, 1.23, 0), "upper_arm.R"),
        "hand.R": ((0.78, 1.23, 0), (0.84, 1.14, 0), "forearm.R"),
        "thigh.L": ((-0.16, 0.95, 0), (-0.20, 0.48, 0), "pelvis"),
        "shin.L": ((-0.20, 0.48, 0), (-0.20, 0.08, 0), "thigh.L"),
        "foot.L": ((-0.20, 0.08, 0), (-0.20, 0.03, 0.22), "shin.L"),
        "thigh.R": ((0.16, 0.95, 0), (0.20, 0.48, 0), "pelvis"),
        "shin.R": ((0.20, 0.48, 0), (0.20, 0.08, 0), "thigh.R"),
        "foot.R": ((0.20, 0.08, 0), (0.20, 0.03, 0.22), "shin.R"),
    }
    armature = make_armature("EnemyBasic_Rig", bones)
    add_sphere("Pelvis", (0, 0.98, 0), (0.28, 0.18, 0.18), dark, armature, "pelvis")
    add_cube("Torso", (0, 1.50, 0), (0.52, 0.68, 0.30), grey, armature, "spine", 0.10)
    add_sphere("Chest", (0, 1.75, 0), (0.32, 0.25, 0.20), grey, armature, "chest")
    add_sphere("Head", (0, 2.18, 0), (0.19, 0.23, 0.19), grey, armature, "head")
    add_cylinder_between("UpperArm.L", (-0.25, 1.78, 0), (-0.58, 1.50, 0), 0.09, grey, armature, "upper_arm.L")
    add_cylinder_between("Forearm.L", (-0.58, 1.50, 0), (-0.78, 1.23, 0), 0.075, dark, armature, "forearm.L")
    add_sphere("Hand.L", (-0.82, 1.18, 0), (0.09, 0.10, 0.09), dark, armature, "hand.L")
    add_cylinder_between("UpperArm.R", (0.25, 1.78, 0), (0.58, 1.50, 0), 0.09, grey, armature, "upper_arm.R")
    add_cylinder_between("Forearm.R", (0.58, 1.50, 0), (0.78, 1.23, 0), 0.075, dark, armature, "forearm.R")
    add_sphere("Hand.R", (0.82, 1.18, 0), (0.09, 0.10, 0.09), dark, armature, "hand.R")
    add_cylinder_between("Thigh.L", (-0.16, 0.95, 0), (-0.20, 0.48, 0), 0.12, grey, armature, "thigh.L")
    add_cylinder_between("Shin.L", (-0.20, 0.48, 0), (-0.20, 0.08, 0), 0.095, dark, armature, "shin.L")
    add_cube("Foot.L", (-0.20, 0.05, 0.11), (0.18, 0.10, 0.32), dark, armature, "foot.L", 0.025)
    add_cylinder_between("Thigh.R", (0.16, 0.95, 0), (0.20, 0.48, 0), 0.12, grey, armature, "thigh.R")
    add_cylinder_between("Shin.R", (0.20, 0.48, 0), (0.20, 0.08, 0), 0.095, dark, armature, "shin.R")
    add_cube("Foot.R", (0.20, 0.05, 0.11), (0.18, 0.10, 0.32), dark, armature, "foot.R", 0.025)

    def idle():
        pose_bone(armature, "spine", 0, rotation=(0, 0, math.radians(-1)))
        pose_bone(armature, "spine", 30, rotation=(0, 0, math.radians(1)))
        pose_bone(armature, "spine", 60, rotation=(0, 0, math.radians(-1)))

    def walk():
        for frame, phase in ((0, 1), (15, -1), (30, 1)):
            swing = math.radians(28) * phase
            pose_bone(armature, "thigh.L", frame, rotation=(swing, 0, 0))
            pose_bone(armature, "shin.L", frame, rotation=(math.radians(-8) * phase, 0, 0))
            pose_bone(armature, "thigh.R", frame, rotation=(-swing, 0, 0))
            pose_bone(armature, "shin.R", frame, rotation=(math.radians(8) * phase, 0, 0))
            pose_bone(armature, "upper_arm.L", frame, rotation=(-swing * 0.7, 0, 0))
            pose_bone(armature, "upper_arm.R", frame, rotation=(swing * 0.7, 0, 0))

    def attack():
        pose_bone(armature, "upper_arm.R", 0)
        pose_bone(armature, "forearm.R", 0)
        pose_bone(armature, "upper_arm.R", 12, rotation=(math.radians(-80), 0, math.radians(-12)))
        pose_bone(armature, "forearm.R", 12, rotation=(math.radians(-45), 0, 0))
        pose_bone(armature, "upper_arm.R", 24)
        pose_bone(armature, "forearm.R", 24)

    def death():
        pose_bone(armature, "root", 0)
        pose_bone(armature, "root", 30, rotation=(0, 0, math.radians(55)), location=(0, 0.05, -0.25))
        pose_bone(armature, "root", 45, rotation=(0, 0, math.radians(90)), location=(0, 0.0, -0.40))

    make_action(armature, "idle", 60, idle)
    make_action(armature, "walk", 30, walk)
    make_action(armature, "attack", 24, attack)
    make_action(armature, "death", 45, death)
    export_asset(armature, "enemy_basic", out_dir)


def parse_args(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--out_dir", default="assets/models")
    return parser.parse_args(argv[argv.index("--") + 1:] if "--" in argv else argv[1:])


def main(argv) -> int:
    args = parse_args(argv)
    out_dir = Path(args.out_dir).resolve()
    build_assault_rifle(out_dir)
    build_enemy(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

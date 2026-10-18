#!/usr/bin/env python3
"""Create the first modular, animation-ready operative mannequin.

The mannequin is intentionally a small foundation rather than a finished
character: head, torso/pelvis, arms, legs, and feet are all readable as a
neutral body base.  Artists can replace or layer armor meshes over the named
bones without changing the skeleton or animation library.

Run with Blender 5.2+ from the repository root::

    Blender -b -P tools/blender/build_operative_mannequin.py -- \
      --out-dir assets/models --preview
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ASSET_NAME = "operative_mannequin"
RIG_NAME = "OperativeMannequin_Rig"
FPS = 30


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    parser.add_argument("--preview", action="store_true")
    return parser.parse_args(argv)


def make_material(name: str, color: tuple[float, float, float, float], roughness: float) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    if shader is not None:
        shader.inputs["Base Color"].default_value = color
        shader.inputs["Roughness"].default_value = roughness
        shader.inputs["Metallic"].default_value = 0.0
    return material


def create_rig() -> bpy.types.Object:
    bones = {
        "Root": ((0.0, 0.0, 0.0), (0.0, 0.0, 0.18), None),
        "Pelvis": ((0.0, 0.0, 0.82), (0.0, 0.0, 1.02), "Root"),
        "Spine": ((0.0, 0.0, 1.02), (0.0, 0.0, 1.33), "Pelvis"),
        "Chest": ((0.0, 0.0, 1.33), (0.0, 0.0, 1.61), "Spine"),
        "Neck": ((0.0, 0.0, 1.61), (0.0, 0.0, 1.74), "Chest"),
        "Head": ((0.0, 0.0, 1.74), (0.0, 0.0, 1.99), "Neck"),
        "LeftUpperArm": ((0.30, 0.0, 1.53), (0.62, 0.0, 1.32), "Chest"),
        "LeftForearm": ((0.62, 0.0, 1.32), (0.76, 0.0, 1.03), "LeftUpperArm"),
        "LeftHand": ((0.76, 0.0, 1.03), (0.78, -0.03, 0.89), "LeftForearm"),
        "RightUpperArm": ((-0.30, 0.0, 1.53), (-0.62, 0.0, 1.32), "Chest"),
        "RightForearm": ((-0.62, 0.0, 1.32), (-0.76, 0.0, 1.03), "RightUpperArm"),
        "RightHand": ((-0.76, 0.0, 1.03), (-0.78, -0.03, 0.89), "RightForearm"),
        "LeftThigh": ((0.17, 0.0, 0.85), (0.20, 0.0, 0.46), "Pelvis"),
        "LeftShin": ((0.20, 0.0, 0.46), (0.20, 0.0, 0.10), "LeftThigh"),
        "LeftFoot": ((0.20, 0.0, 0.10), (0.20, -0.24, 0.05), "LeftShin"),
        "RightThigh": ((-0.17, 0.0, 0.85), (-0.20, 0.0, 0.46), "Pelvis"),
        "RightShin": ((-0.20, 0.0, 0.46), (-0.20, 0.0, 0.10), "RightThigh"),
        "RightFoot": ((-0.20, 0.0, 0.10), (-0.20, -0.24, 0.05), "RightShin"),
    }
    bpy.ops.object.armature_add(enter_editmode=True)
    armature = bpy.context.active_object
    armature.name = RIG_NAME
    armature.data.name = "OperativeMannequin_Skeleton"
    edit_bones = armature.data.edit_bones
    edit_bones.remove(edit_bones[0])
    created: dict[str, bpy.types.EditBone] = {}
    for name, (head, tail, parent) in bones.items():
        bone = edit_bones.new(name)
        bone.head = Vector(head)
        bone.tail = Vector(tail)
        if parent is not None:
            bone.parent = created[parent]
        created[name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    armature.show_in_front = True
    return armature


def bind_to_bone(obj: bpy.types.Object, armature: bpy.types.Object, bone_name: str) -> None:
    obj.parent = armature
    modifier = obj.modifiers.new("Mannequin rig", "ARMATURE")
    modifier.object = armature
    group = obj.vertex_groups.new(name=bone_name)
    group.add([vertex.index for vertex in obj.data.vertices], 1.0, "REPLACE")


def smooth_mesh(obj: bpy.types.Object, material: bpy.types.Material, armature: bpy.types.Object, bone_name: str,
                smooth: bool = True) -> None:
    obj.data.materials.append(material)
    if smooth:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    bind_to_bone(obj, armature, bone_name)


def add_ellipsoid(name: str, location: tuple[float, float, float], scale: tuple[float, float, float],
                  material: bpy.types.Material, armature: bpy.types.Object, bone_name: str) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=18, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    smooth_mesh(obj, material, armature, bone_name)
    return obj


def add_capsule(name: str, start: tuple[float, float, float], end: tuple[float, float, float], radius: float,
                material: bpy.types.Material, armature: bpy.types.Object, bone_name: str) -> bpy.types.Object:
    start_v = Vector(start)
    end_v = Vector(end)
    direction = end_v - start_v
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=14, location=(start_v + end_v) * 0.5)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (radius, radius, max(direction.length * 0.62, radius))
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    smooth_mesh(obj, material, armature, bone_name)
    return obj


def add_rounded_box(name: str, location: tuple[float, float, float], dimensions: tuple[float, float, float],
                    material: bpy.types.Material, armature: bpy.types.Object, bone_name: str,
                    bevel: float, rotation=None) -> bpy.types.Object:
    """Add a chamfered hard-surface part rather than an inflatable body segment."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    if rotation is not None:
        obj.rotation_mode = "QUATERNION"
        obj.rotation_quaternion = rotation
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    modifier = obj.modifiers.new("Soft manufactured edges", "BEVEL")
    modifier.width = bevel
    modifier.segments = 3
    modifier.limit_method = "ANGLE"
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    smooth_mesh(obj, material, armature, bone_name)
    return obj


def add_limb_shell(name: str, start: tuple[float, float, float], end: tuple[float, float, float],
                   width: float, depth: float, material: bpy.types.Material, armature: bpy.types.Object,
                   bone_name: str) -> bpy.types.Object:
    """Create a slender, manufactured limb shell aligned to a rig bone."""
    start_v = Vector(start)
    end_v = Vector(end)
    direction = end_v - start_v
    shell_length = max(direction.length * 0.80, 0.06)
    return add_rounded_box(
        name,
        tuple((start_v + end_v) * 0.5),
        (width, depth, shell_length),
        material,
        armature,
        bone_name,
        min(width, depth) * 0.28,
        direction.to_track_quat("Z", "Y"),
    )


def build_body(armature: bpy.types.Object) -> None:
    # These are deliberately clean, neutral materials.  The head is a separate
    # skin material region so a player complexion/hair system can later replace
    # it without changing body proportions or any animation binding.
    shell = make_material("Mannequin Shell", (0.74, 0.77, 0.78, 1.0), 0.38)
    skin = make_material("Skin Material Region", (0.69, 0.72, 0.73, 1.0), 0.54)
    joint = make_material("Joint Polymer", (0.035, 0.052, 0.062, 1.0), 0.34)
    visor = make_material("Visor Placeholder", (0.035, 0.20, 0.24, 1.0), 0.22)

    # A shell construction keeps the silhouette deliberately lean and makes it
    # legible where detachable armour parts will sit.  Gaps expose compact dark
    # joints instead of the large round pieces used by the first prototype.
    add_rounded_box("Pelvis shell", (0.0, 0.0, 0.91), (0.47, 0.30, 0.23), shell, armature, "Pelvis", 0.055)
    add_rounded_box("Abdomen shell", (0.0, 0.0, 1.18), (0.49, 0.29, 0.31), shell, armature, "Spine", 0.070)
    add_rounded_box("Chest shell", (0.0, 0.0, 1.45), (0.70, 0.32, 0.35), shell, armature, "Chest", 0.075)
    add_rounded_box("Left chest plate", (0.17, -0.174, 1.49), (0.28, 0.048, 0.20), skin, armature, "Chest", 0.026)
    add_rounded_box("Right chest plate", (-0.17, -0.174, 1.49), (0.28, 0.048, 0.20), skin, armature, "Chest", 0.026)
    add_rounded_box("Lower torso plate", (0.0, -0.164, 1.28), (0.36, 0.044, 0.10), skin, armature, "Spine", 0.020)
    add_ellipsoid("Neck joint", (0.0, 0.0, 1.68), (0.090, 0.090, 0.105), joint, armature, "Neck")
    add_ellipsoid("Head base", (0.0, -0.005, 1.87), (0.185, 0.175, 0.245), skin, armature, "Head")
    add_rounded_box("Face visor placeholder", (0.0, -0.174, 1.87), (0.175, 0.024, 0.075), visor, armature, "Head", 0.015)

    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        add_ellipsoid(f"{side} shoulder joint", (0.34 * sign, 0.0, 1.53), (0.085, 0.095, 0.080), joint, armature, "Chest")
        add_limb_shell(f"{side} upper arm shell", (0.40 * sign, 0.0, 1.46), (0.59 * sign, 0.0, 1.34),
                       0.135, 0.145, shell, armature, f"{side}UpperArm")
        add_ellipsoid(f"{side} elbow joint", (0.62 * sign, 0.0, 1.32), (0.074, 0.078, 0.074), joint, armature, f"{side}UpperArm")
        add_limb_shell(f"{side} forearm shell", (0.65 * sign, 0.0, 1.27), (0.74 * sign, 0.0, 1.07),
                       0.118, 0.130, skin, armature, f"{side}Forearm")
        add_rounded_box(f"{side} hand shell", (0.77 * sign, -0.02, 0.98), (0.115, 0.075, 0.145), skin,
                        armature, f"{side}Hand", 0.028)

        add_limb_shell(f"{side} thigh shell", (0.17 * sign, 0.0, 0.81), (0.20 * sign, 0.0, 0.49),
                       0.205, 0.190, shell, armature, f"{side}Thigh")
        add_ellipsoid(f"{side} knee joint", (0.20 * sign, 0.0, 0.46), (0.105, 0.095, 0.075), joint, armature, f"{side}Thigh")
        add_limb_shell(f"{side} shin shell", (0.20 * sign, 0.0, 0.41), (0.20 * sign, 0.0, 0.13),
                       0.160, 0.155, skin, armature, f"{side}Shin")
        add_rounded_box(f"{side} foot shell", (0.20 * sign, -0.13, 0.075), (0.185, 0.315, 0.105), shell,
                        armature, f"{side}Foot", 0.036)


def reset_pose(armature: bpy.types.Object) -> None:
    for pose_bone in armature.pose.bones:
        pose_bone.rotation_mode = "XYZ"
        pose_bone.location = (0.0, 0.0, 0.0)
        pose_bone.rotation_euler = (0.0, 0.0, 0.0)
        pose_bone.scale = (1.0, 1.0, 1.0)


def key_all(armature: bpy.types.Object, frame: int) -> None:
    bpy.context.scene.frame_set(frame)
    for pose_bone in armature.pose.bones:
        pose_bone.keyframe_insert(data_path="location", frame=frame)
        pose_bone.keyframe_insert(data_path="rotation_euler", frame=frame)
        pose_bone.keyframe_insert(data_path="scale", frame=frame)


def rotate(armature: bpy.types.Object, bone: str, degrees: tuple[float, float, float]) -> None:
    armature.pose.bones[bone].rotation_euler = tuple(math.radians(value) for value in degrees)


def build_action(armature: bpy.types.Object, name: str, end: int, pose_fn) -> bpy.types.Action:
    armature.animation_data_create()
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    armature.animation_data.action = action
    reset_pose(armature)
    pose_fn()
    key_all(armature, 1)
    key_all(armature, end)
    return action


def build_animations(armature: bpy.types.Object) -> list[bpy.types.Action]:
    def profile() -> None:
        rotate(armature, "LeftUpperArm", (0.0, 0.0, 7.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, -7.0))

    def crouch() -> None:
        armature.pose.bones["Pelvis"].location.z = -0.27
        rotate(armature, "LeftThigh", (43.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (43.0, 0.0, 0.0))
        rotate(armature, "LeftShin", (-68.0, 0.0, 0.0))
        rotate(armature, "RightShin", (-68.0, 0.0, 0.0))
        rotate(armature, "Spine", (-12.0, 0.0, 0.0))

    def jump() -> None:
        armature.pose.bones["Root"].location.z = 0.18
        rotate(armature, "LeftThigh", (-22.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (-22.0, 0.0, 0.0))
        rotate(armature, "LeftUpperArm", (0.0, 0.0, -24.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, 24.0))

    def airborne() -> None:
        armature.pose.bones["Root"].location.z = 0.28
        rotate(armature, "LeftThigh", (13.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (13.0, 0.0, 0.0))
        rotate(armature, "LeftShin", (-18.0, 0.0, 0.0))
        rotate(armature, "RightShin", (-18.0, 0.0, 0.0))
        rotate(armature, "LeftUpperArm", (0.0, 0.0, -14.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, 14.0))

    def float_pose() -> None:
        armature.pose.bones["Root"].location.z = 0.30
        rotate(armature, "LeftUpperArm", (0.0, 0.0, -57.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, 57.0))
        rotate(armature, "LeftForearm", (0.0, 0.0, -17.0))
        rotate(armature, "RightForearm", (0.0, 0.0, 17.0))
        rotate(armature, "LeftThigh", (10.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (10.0, 0.0, 0.0))

    def land() -> None:
        armature.pose.bones["Pelvis"].location.z = -0.12
        rotate(armature, "LeftThigh", (25.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (25.0, 0.0, 0.0))
        rotate(armature, "LeftShin", (-36.0, 0.0, 0.0))
        rotate(armature, "RightShin", (-36.0, 0.0, 0.0))

    def walk() -> None:
        rotate(armature, "LeftThigh", (21.0, 0.0, 0.0))
        rotate(armature, "RightThigh", (-21.0, 0.0, 0.0))
        rotate(armature, "LeftUpperArm", (0.0, 0.0, -16.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, 16.0))

    def dance() -> None:
        rotate(armature, "Spine", (0.0, 0.0, 13.0))
        rotate(armature, "LeftUpperArm", (0.0, 0.0, -78.0))
        rotate(armature, "RightUpperArm", (0.0, 0.0, 38.0))
        rotate(armature, "LeftThigh", (0.0, 0.0, 14.0))
        rotate(armature, "RightThigh", (0.0, 0.0, -14.0))

    return [
        build_action(armature, "profile_stance", 48, profile),
        build_action(armature, "crouch_idle", 48, crouch),
        build_action(armature, "jump_takeoff", 18, jump),
        build_action(armature, "airborne_loop", 48, airborne),
        build_action(armature, "float_loop", 48, float_pose),
        build_action(armature, "land_soft", 18, land),
        build_action(armature, "locomotion_walk", 30, walk),
        build_action(armature, "dance_placeholder", 48, dance),
    ]


def add_preview_scene() -> None:
    bpy.ops.object.light_add(type="AREA", location=(3.0, -4.0, 4.0))
    bpy.context.active_object.data.energy = 800.0
    bpy.context.active_object.data.shape = "DISK"
    bpy.context.active_object.data.size = 4.0
    bpy.ops.object.light_add(type="AREA", location=(-2.5, -1.0, 2.0))
    bpy.context.active_object.data.energy = 380.0
    bpy.context.active_object.data.color = (0.20, 0.55, 0.72)
    bpy.context.active_object.data.size = 3.0
    bpy.ops.mesh.primitive_plane_add(size=12.0, location=(0.0, 0.0, 0.0))
    floor = bpy.context.active_object
    floor.name = "Preview ground (not exported)"
    floor.data.materials.append(make_material("Preview Ground", (0.018, 0.025, 0.03, 1.0), 0.9))
    floor["preview_only"] = True
    bpy.ops.object.camera_add(location=(0.0, -6.7, 1.15))
    camera = bpy.context.active_object
    camera.name = "Preview camera"
    camera.rotation_euler = (Vector((0.0, 0.0, 1.05)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 58.0
    camera["preview_only"] = True
    bpy.context.scene.camera = camera


def export_asset(armature: bpy.types.Object, actions: list[bpy.types.Action], out_dir: Path, preview: bool) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.context.scene.render.resolution_x = 900
    bpy.context.scene.render.resolution_y = 900
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.image_settings.file_format = "PNG"
    bpy.context.scene.render.filepath = str((out_dir / f"{ASSET_NAME}_preview.png").resolve())
    armature.animation_data.action = next(action for action in actions if action.name == "profile_stance")
    bpy.context.scene.frame_set(1)
    add_preview_scene()
    bpy.ops.wm.save_as_mainfile(filepath=str((out_dir / f"{ASSET_NAME}.blend").resolve()))
    if preview:
        bpy.ops.render.render(write_still=True)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if not obj.get("preview_only", False):
            obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.gltf(
        filepath=str((out_dir / f"{ASSET_NAME}.gltf").resolve()),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=True,
        export_frame_step=1,
        export_skins=True,
        export_materials="EXPORT",
    )


def main() -> None:
    args = parse_args()
    if bpy.app.background:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    engine_values = {item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engine_values else "BLENDER_EEVEE"
    bpy.context.scene.render.film_transparent = False
    bpy.context.scene.render.image_settings.color_mode = "RGBA"
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.fps = FPS
    armature = create_rig()
    build_body(armature)
    actions = build_animations(armature)
    export_asset(armature, actions, args.out_dir, args.preview)
    print(f"Generated {args.out_dir / (ASSET_NAME + '.blend')}")
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

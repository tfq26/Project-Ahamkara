#!/usr/bin/env python3
"""Build the Caligo field-suit character and attach CMU Charleston motion.

Run with Blender 5.1+ from the repository root::

    /Applications/Blender.app/Contents/MacOS/Blender -b \
      -P tools/blender/build_character_showcase.py -- \
      --bvh /path/to/93_02.bvh --out-dir assets/models

The resulting asset is intentionally a stylised, production-shaped mannequin:
smooth anatomy, layered field armour, a complete humanoid skeleton, and an
in-place motion-capture dance.  It is the first character-pipeline vertical
slice, not final character art.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ASSET_NAME = "caligo_field_suit"
MOCAP_CLIP_NAME = "dance_charleston"
MOCAP_SCALE = 0.085


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--bvh", required=True, type=Path)
    parser.add_argument("--walk-bvh", type=Path)
    parser.add_argument("--run-bvh", type=Path)
    parser.add_argument("--out-dir", default=Path("assets/models"), type=Path)
    parser.add_argument("--preview", action="store_true")
    return parser.parse_args(argv)


def make_material(
    name: str,
    color: tuple[float, float, float, float],
    *,
    metallic: float = 0.0,
    roughness: float = 0.65,
):
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    if shader is not None:
        shader.inputs["Base Color"].default_value = color
        shader.inputs["Metallic"].default_value = metallic
        shader.inputs["Roughness"].default_value = roughness
    return material


def apply_geometry(obj) -> None:
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for modifier in list(obj.modifiers):
        if modifier.type != "ARMATURE":
            bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    obj.select_set(False)


def bind_to_bone(obj, armature, bone_name: str) -> None:
    obj.parent = armature
    modifier = obj.modifiers.new("Character rig", "ARMATURE")
    modifier.object = armature
    group = obj.vertex_groups.new(name=bone_name)
    group.add([vertex.index for vertex in obj.data.vertices], 1.0, "REPLACE")


def add_ellipsoid(
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    material,
    armature,
    bone_name: str,
    *,
    segments: int = 32,
    rings: int = 20,
):
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=segments,
        ring_count=rings,
        location=location,
    )
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    apply_geometry(obj)
    bind_to_bone(obj, armature, bone_name)
    return obj


def add_limb(
    name: str,
    start: Vector,
    end: Vector,
    radius: float,
    material,
    armature,
    bone_name: str,
    *,
    taper: float = 0.86,
):
    direction = end - start
    midpoint = (start + end) * 0.5
    bpy.ops.mesh.primitive_cone_add(
        vertices=28,
        radius1=radius * taper,
        radius2=radius,
        depth=max(direction.length * 1.06, radius * 2.0),
        location=midpoint,
    )
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    bevel = obj.modifiers.new("Rounded joint edges", "BEVEL")
    bevel.width = radius * 0.42
    bevel.segments = 4
    obj.data.materials.append(material)
    apply_geometry(obj)
    bind_to_bone(obj, armature, bone_name)
    return obj


def add_rounded_box(
    name: str,
    location: tuple[float, float, float],
    dimensions: tuple[float, float, float],
    material,
    armature,
    bone_name: str,
    *,
    bevel_width: float,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bevel = obj.modifiers.new("Soft armour edges", "BEVEL")
    bevel.width = bevel_width
    bevel.segments = 5
    obj.data.materials.append(material)
    apply_geometry(obj)
    bind_to_bone(obj, armature, bone_name)
    return obj


def bone_points(armature, name: str) -> tuple[Vector, Vector]:
    bone = armature.data.bones[name]
    return bone.head_local.copy(), bone.tail_local.copy()


def normalize_root_motion(armature, clip_name: str = MOCAP_CLIP_NAME) -> None:
    """Keep an authored mocap clip in place while preserving vertical footwork.

    Ahamkara's gameplay transform owns world-space movement. BVH captures carry
    performer travel in the horizontal X/Y channels, so subtracting only their
    initial values still lets the mesh walk away from the entity. Zero those
    channels completely; keep Z so steps, crouches, and jumps remain readable.
    """
    action = armature.animation_data.action
    if action is None:
        raise RuntimeError("BVH import did not produce an action")
    action.name = clip_name
    root_path = 'pose.bones["Hips"].location'
    if hasattr(action, "fcurves"):
        curves = action.fcurves
    else:
        # Blender 4.4+ stores curves in a layered Action channel bag.
        curves = []
        for layer in action.layers:
            for strip in layer.strips:
                for slot in action.slots:
                    channel_bag = strip.channelbag(slot)
                    if channel_bag is not None:
                        curves.extend(channel_bag.fcurves)
    for curve in curves:
        if curve.data_path != root_path or curve.array_index == 2:
            continue
        points = curve.keyframe_points
        if not points:
            continue
        for point in points:
            point.co.y = 0.0
            point.handle_left.y = 0.0
            point.handle_right.y = 0.0


def import_motion(path: Path, clip_name: str):
    """Import a compatible CMU BVH and keep its Action for the field-suit rig."""
    if not path.is_file():
        raise FileNotFoundError(path)
    bpy.ops.import_anim.bvh(
        filepath=str(path.resolve()),
        global_scale=MOCAP_SCALE,
        update_scene_fps=True,
        update_scene_duration=True,
        rotate_mode="QUATERNION",
    )
    source_armature = bpy.context.object
    normalize_root_motion(source_armature, clip_name)
    action = source_armature.animation_data.action
    action.use_fake_user = True
    bpy.data.objects.remove(source_armature, do_unlink=True)
    return action


def add_character_geometry(armature) -> None:
    sand = make_material("Caligo Sand Ceramic", (0.62, 0.37, 0.17, 1.0), metallic=0.08, roughness=0.54)
    pale = make_material("Weathered Bone Ceramic", (0.70, 0.64, 0.52, 1.0), metallic=0.04, roughness=0.62)
    graphite = make_material("Graphite Pressure Suit", (0.035, 0.045, 0.055, 1.0), metallic=0.18, roughness=0.38)
    visor = make_material("Magnetic Glass Visor", (0.025, 0.20, 0.23, 1.0), metallic=0.48, roughness=0.17)
    copper = make_material("Grounding Copper", (0.48, 0.18, 0.055, 1.0), metallic=0.72, roughness=0.31)

    # Soft pressure suit underlayer. Overlapping smooth volumes hide rigid
    # armour seams and retain readable anatomy during energetic mocap.
    add_ellipsoid("Pelvis underlayer", (0.0, 0.0, -0.01), (0.25, 0.18, 0.20), graphite, armature, "Hips")
    add_ellipsoid("Abdomen underlayer", (0.0, 0.0, 0.20), (0.25, 0.17, 0.29), graphite, armature, "Spine")
    add_ellipsoid("Chest underlayer", (0.0, 0.0, 0.38), (0.34, 0.20, 0.28), graphite, armature, "Spine1")
    add_limb("Neck seal", *bone_points(armature, "Neck1"), 0.09, graphite, armature, "Neck1", taper=1.0)

    # Asymmetrical field armour gives the silhouette authored detail without
    # requiring textures for this vertical slice.
    add_rounded_box(
        "Chest ceramic", (0.0, -0.14, 0.39), (0.52, 0.13, 0.42), pale, armature, "Spine1", bevel_width=0.055
    )
    add_rounded_box(
        "Abdomen grounding plate",
        (0.0, -0.155, 0.15),
        (0.34, 0.075, 0.16),
        copper,
        armature,
        "Spine",
        bevel_width=0.025,
    )
    add_rounded_box(
        "Left hip shell",
        (0.17, -0.035, -0.06),
        (0.23, 0.29, 0.27),
        sand,
        armature,
        "LHipJoint",
        bevel_width=0.055,
        rotation=(0.0, 0.0, math.radians(-7.0)),
    )
    add_rounded_box(
        "Right hip shell",
        (-0.17, -0.035, -0.06),
        (0.23, 0.29, 0.27),
        sand,
        armature,
        "RHipJoint",
        bevel_width=0.055,
        rotation=(0.0, 0.0, math.radians(7.0)),
    )

    for side, _prefix in (("Left", "L"), ("Right", "R")):
        upper_leg = f"{side}UpLeg"
        lower_leg = f"{side}Leg"
        foot = f"{side}Foot"
        arm = f"{side}Arm"
        forearm = f"{side}ForeArm"
        hand = f"{side}Hand"

        add_limb(f"{side} thigh suit", *bone_points(armature, upper_leg), 0.155, graphite, armature, upper_leg)
        add_limb(
            f"{side} thigh armour", *bone_points(armature, upper_leg), 0.132, sand, armature, upper_leg, taper=0.74
        )
        add_limb(f"{side} shin suit", *bone_points(armature, lower_leg), 0.125, graphite, armature, lower_leg)
        add_limb(f"{side} shin armour", *bone_points(armature, lower_leg), 0.103, pale, armature, lower_leg, taper=0.68)

        foot_head, foot_tail = bone_points(armature, foot)
        foot_mid = (foot_head + foot_tail) * 0.5
        add_rounded_box(
            f"{side} grounded boot", tuple(foot_mid), (0.23, 0.40, 0.17), graphite, armature, foot, bevel_width=0.06
        )
        add_rounded_box(
            f"{side} boot plate",
            tuple(foot_mid + Vector((0.0, -0.07, 0.045))),
            (0.19, 0.26, 0.07),
            copper,
            armature,
            foot,
            bevel_width=0.025,
        )

        add_limb(f"{side} upper arm suit", *bone_points(armature, arm), 0.105, graphite, armature, arm)
        add_limb(f"{side} upper arm shell", *bone_points(armature, arm), 0.088, sand, armature, arm, taper=0.72)
        add_limb(f"{side} forearm suit", *bone_points(armature, forearm), 0.087, graphite, armature, forearm)
        add_limb(f"{side} forearm bracer", *bone_points(armature, forearm), 0.073, pale, armature, forearm, taper=0.70)

        hand_head, _ = bone_points(armature, hand)
        add_ellipsoid(
            f"{side} glove", tuple(hand_head), (0.105, 0.07, 0.12), graphite, armature, hand, segments=24, rings=16
        )

    # Shoulder shells and helmet are deliberately separate so arm elevation
    # reads clearly in the Charleston clip.
    for side, sign in (("Left", 1.0), ("Right", -1.0)):
        add_ellipsoid(
            f"{side} shoulder shell",
            (0.29 * sign, -0.015, 0.43),
            (0.18, 0.20, 0.15),
            sand,
            armature,
            f"{side}Shoulder",
        )

    add_ellipsoid("Helmet", (0.0, 0.0, 0.69), (0.22, 0.19, 0.26), pale, armature, "Head")
    add_ellipsoid("Visor", (0.0, -0.176, 0.71), (0.155, 0.035, 0.115), visor, armature, "Head", segments=32, rings=20)
    add_rounded_box(
        "Helmet grounding rail", (0.0, 0.17, 0.69), (0.055, 0.035, 0.32), copper, armature, "Head", bevel_width=0.015
    )


def add_preview_scene(armature) -> None:
    bpy.ops.object.light_add(type="AREA", location=(3.0, -4.0, 4.5))
    key = bpy.context.object
    key.name = "Preview key"
    key.data.energy = 950.0
    key.data.shape = "DISK"
    key.data.size = 4.0
    key.rotation_euler = (math.radians(28), 0.0, math.radians(32))

    bpy.ops.object.light_add(type="AREA", location=(-3.0, 1.0, 2.0))
    fill = bpy.context.object
    fill.name = "Preview fill"
    fill.data.energy = 480.0
    fill.data.color = (0.22, 0.48, 0.62)
    fill.data.size = 3.0

    bpy.ops.mesh.primitive_plane_add(size=12.0, location=(0.0, 0.0, -1.43))
    ground = bpy.context.object
    ground.name = "Preview ground (not exported)"
    ground.data.materials.append(make_material("Preview ground", (0.018, 0.022, 0.027, 1.0), roughness=0.9))

    bpy.ops.object.camera_add(location=(3.8, -6.8, 1.5))
    camera = bpy.context.object
    camera.name = "Preview camera"
    direction = Vector((0.0, 0.0, -0.25)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 58
    bpy.context.scene.camera = camera
    bpy.context.scene.render.resolution_x = 900
    bpy.context.scene.render.resolution_y = 900
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.image_settings.file_format = "PNG"
    bpy.context.scene.render.filepath = str(Path("assets/models/caligo_field_suit_preview.png").resolve())

    # Do not let preview-only objects leak into the game export.
    for obj in (key, fill, ground, camera):
        obj["preview_only"] = True


def export_asset(armature, actions, out_dir: Path, render_preview: bool) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    blend_path = (out_dir / f"{ASSET_NAME}.blend").resolve()
    gltf_path = (out_dir / f"{ASSET_NAME}.gltf").resolve()

    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_set(140)
    armature.animation_data_create()
    for action in actions:
        track = armature.animation_data.nla_tracks.new()
        track.name = action.name
        track.strips.new(action.name, int(action.frame_range[0]), action)
        track.mute = True
    armature.animation_data.action = actions[0]
    add_preview_scene(armature)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    if render_preview:
        bpy.ops.render.render(write_still=True)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if not obj.get("preview_only", False):
            obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_frame_step=4,
        export_force_sampling=True,
        export_skins=True,
        export_materials="EXPORT",
    )
    print(f"Generated {blend_path}")
    print(f"Generated {gltf_path}")


def main() -> None:
    args = parse_args()
    if not args.bvh.is_file():
        raise FileNotFoundError(args.bvh)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_anim.bvh(
        filepath=str(args.bvh.resolve()),
        global_scale=MOCAP_SCALE,
        update_scene_fps=True,
        update_scene_duration=True,
        rotate_mode="QUATERNION",
    )
    armature = bpy.context.object
    armature.name = "CaligoFieldSuit_Rig"
    armature.data.name = "CaligoFieldSuit_Skeleton"
    normalize_root_motion(armature)
    actions = [armature.animation_data.action]
    if args.walk_bvh is not None:
        actions.append(import_motion(args.walk_bvh, "locomotion_walk"))
    if args.run_bvh is not None:
        actions.append(import_motion(args.run_bvh, "locomotion_run"))
    armature.animation_data.action = actions[0]
    add_character_geometry(armature)
    export_asset(armature, actions, args.out_dir, args.preview)


if __name__ == "__main__":
    main()

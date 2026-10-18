#!/usr/bin/env python3
"""Bake a single weapon-ready Mixamo pose into the operative Character View mesh.

This is deliberately a presentation export.  The live gameplay character
continues to use its own skinned asset, while this evaluated mesh gives the
Character View a dependable, full-body rifle-ready silhouette until its
skeletal path is ready for every imported animation.

Run from the repository root:

  Blender -b assets/models/operative_human_base.blend \
    -P tools/blender/export_operative_rifle_idle_profile.py -- \
    --source assets/source_animations/rifle_idle_mixamo.fbx --out-dir assets/models
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

ASSET_NAME = "operative_human_rifle_idle_static"
TARGET_RIG = "Ahamkara_Operative_GameRig"
TARGET_MESH = "Ahamkara_Operative_Human_Base"
SOURCE_PREFIX = "mixamorig:"
CARBINE_ASSET = Path("assets/models/operative_carbine_tp.gltf")

# Rifle-idle uses an authored, two-hand contact pose. The downloaded Mixamo
# clip remains source material for the animation-retargeting pass, but its
# local bone rotations cannot safely be copied onto this differently-oriented
# MPFB skeleton.  The presentation export must only contain a pose that has
# been visually validated in the operative's own rig.
# The real M4A1 is placed rigidly from its stock into the operative's right
# shoulder. Arm IK then moves the hands to the weapon's actual grip sockets;
# the weapon is never stretched to bridge arbitrary hand targets.
STOCK_SHOULDER = Vector((-0.245, 0.0, 0.055))
RIGHT_HAND_SOCKET = Vector((0.0, 0.0, 0.0))
LEFT_HAND_SOCKET = Vector((0.255, 0.0, 0.075))
RIGHT_WRIST_FROM_GRIP = Vector((-0.010, -0.035, -0.055))
LEFT_WRIST_FROM_GRIP = Vector((-0.020, -0.040, -0.060))
SHOULDER_CONTACT = Vector((-0.20, -0.20, 1.26))
WEAPON_FORWARD = Vector((0.993, 0.10, 0.05)).normalized()

# The imported operative uses mirror-imaged hand chains.  Finger flexion must
# therefore use opposite signs on each hand even though both hands close
# around the same weapon.  Values are authored for this exact carbine grip;
# they are intentionally stored as a named profile rather than mixed into the
# IK solve so a future rifle animation exporter can reuse the pose contract.
RIFLE_GRIP_PROFILE = {
    "right": {
        "palm_roll": 0.32,
        "finger_curls": {
            "index": (0.20, 0.46, 0.34),
            "middle": (0.48, 0.76, 0.54),
            "ring": (0.56, 0.82, 0.58),
            "pinky": (0.62, 0.86, 0.60),
        },
    },
    "left": {
        "palm_roll": -0.32,
        "finger_curls": {
            "index": (0.42, 0.70, 0.50),
            "middle": (0.52, 0.80, 0.58),
            "ring": (0.58, 0.84, 0.60),
            "pinky": (0.64, 0.88, 0.62),
        },
    },
}


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    parser.add_argument("--frame", type=int, default=20, help="A stable frame near the middle of the idle loop.")
    return parser.parse_args(argv)


def reset_pose(rig: bpy.types.Object) -> None:
    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        bone.scale = (1.0, 1.0, 1.0)


def validate_motion_source(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(path)


def apply_local_rotation(
    bone: bpy.types.PoseBone,
    axis: Vector,
    radians: float,
) -> None:
    """Apply an authored local-space delta without altering the rest pose."""
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = Quaternion(axis, radians) @ bone.rotation_quaternion


def author_rifle_grip(target: bpy.types.Object) -> None:
    """Add the palm orientation and finger contact missing from an IK-only pose.

    The weapon and wrist positions are established by the arm IK constraints.
    This pass deliberately happens afterwards: it supplies the human-authored
    contact information that a two-bone solve cannot infer from a socket.
    """
    for side, sign in (("right", -1.0), ("left", 1.0)):
        suffix = "r" if side == "right" else "l"
        profile = RIFLE_GRIP_PROFILE[side]
        hand = target.pose.bones[f"hand_{suffix}"]
        # This profile uses local-space flexion deltas. The mirrored skeleton
        # requires the opposite sign per side.
        apply_local_rotation(hand, Vector((0.0, 1.0, 0.0)), profile["palm_roll"])
        for finger, curls in profile["finger_curls"].items():
            for joint, curl in enumerate(curls, start=1):
                finger_bone = target.pose.bones[f"{finger}_{joint:02d}_{suffix}"]
                apply_local_rotation(finger_bone, Vector((1.0, 0.0, 0.0)), sign * curl)


def weapon_placement() -> Matrix:
    """Seat the stock at the right shoulder with a shallow across-body cant."""
    world_up = Vector((0.0, 0.0, 1.0))
    y_axis = world_up.cross(WEAPON_FORWARD).normalized()
    z_axis = WEAPON_FORWARD.cross(y_axis).normalized()
    orientation = Matrix(
        (
            (WEAPON_FORWARD.x, y_axis.x, z_axis.x, 0.0),
            (WEAPON_FORWARD.y, y_axis.y, z_axis.y, 0.0),
            (WEAPON_FORWARD.z, y_axis.z, z_axis.z, 0.0),
            (0.0, 0.0, 0.0, 1.0),
        )
    )
    return Matrix.Translation(SHOULDER_CONTACT) @ orientation @ Matrix.Translation(-STOCK_SHOULDER)


def pose_rifle_ready(
    target: bpy.types.Object,
    frame: int,
    placement: Matrix,
) -> tuple[Vector, Vector]:
    """Author a stable, validated two-hand weapon-ready pose on the target rig."""
    bpy.context.scene.frame_set(frame)
    reset_pose(target)
    right_grip = placement @ (RIGHT_HAND_SOCKET + RIGHT_WRIST_FROM_GRIP)
    left_grip = placement @ (LEFT_HAND_SOCKET + LEFT_WRIST_FROM_GRIP)
    for name, location in (("Profile Right Grip", right_grip), ("Profile Left Guard", left_grip)):
        bpy.ops.object.empty_add(type="PLAIN_AXES", location=location)
        marker = bpy.context.object
        marker.name = name
        marker.empty_display_size = 0.04
        marker.hide_render = True
    bpy.context.view_layer.update()
    for bone_name, marker_name in (("lowerarm_r", "Profile Right Grip"), ("lowerarm_l", "Profile Left Guard")):
        bone = target.pose.bones[bone_name]
        constraint = bone.constraints.new("IK")
        constraint.target = bpy.data.objects[marker_name]
        constraint.chain_count = 2
        constraint.iterations = 128
    bpy.context.view_layer.update()
    author_rifle_grip(target)
    bpy.context.view_layer.update()
    left_hand = target.matrix_world @ target.pose.bones["hand_l"].head
    right_hand = target.matrix_world @ target.pose.bones["hand_r"].head
    print(f"hand_l at {tuple(round(value, 3) for value in left_hand)}")
    print(f"hand_r at {tuple(round(value, 3) for value in right_hand)}")
    return right_hand, left_hand


def evaluated_profile(source: bpy.types.Object) -> bpy.types.Object:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = source.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=depsgraph)
    for material in source.data.materials:
        mesh.materials.append(material)
    profile = bpy.data.objects.new(ASSET_NAME, mesh)
    bpy.context.collection.objects.link(profile)
    profile.matrix_world = source.matrix_world.copy()
    return profile


def add_presentation_rifle(placement: Matrix) -> list[bpy.types.Object]:
    """Attach the canonical carbine using its fixed shoulder placement."""
    if not CARBINE_ASSET.is_file():
        raise FileNotFoundError(CARBINE_ASSET)
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(CARBINE_ASSET.resolve()))
    rifle_parts = [
        obj
        for obj in bpy.context.scene.objects
        if obj not in before and obj.type == "MESH" and obj.name == "operative_carbine_tp"
    ]
    if len(rifle_parts) != 1:
        raise RuntimeError("Expected the canonical third-person carbine mesh")

    for part in rifle_parts:
        part.matrix_world = placement @ part.matrix_world
        part.name = f"Profile Rifle {part.name}"
    bpy.context.view_layer.update()
    return rifle_parts


def export(objects: list[bpy.types.Object], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    # The compiled-mesh path consumes vertex data rather than retaining glTF
    # node transforms.  Bake the presentation transforms so rifle placement is
    # identical in Blender and in the game.
    from mathutils import Matrix

    for obj in objects:
        obj.data.transform(obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)

    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.gltf(
        filepath=str((out_dir / f"{ASSET_NAME}.gltf").resolve()),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=False,
        export_materials="EXPORT",
    )


def main() -> None:
    args = parse_args()
    target = bpy.data.objects.get(TARGET_RIG)
    mesh = bpy.data.objects.get(TARGET_MESH)
    if target is None or target.type != "ARMATURE" or mesh is None or mesh.type != "MESH":
        raise RuntimeError("Expected the operative rig and human mesh in the opened blend file")
    validate_motion_source(args.source)
    placement = weapon_placement()
    pose_rifle_ready(target, args.frame, placement)
    profile = evaluated_profile(mesh)
    rifle_parts = add_presentation_rifle(placement)
    export([profile, *rifle_parts], args.out_dir)
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

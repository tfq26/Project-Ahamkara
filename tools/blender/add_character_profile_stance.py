#!/usr/bin/env python3
"""Add and export the Caligo field suit's neutral Character View stance.

Run through Blender 5.2+ from the repository root::

    Blender -b assets/models/caligo_field_suit.blend \
      -P tools/blender/add_character_profile_stance.py -- \
      --out-dir /private/tmp/caligo-profile-stance --render-preview

The stance is deliberately authored rather than sourced from a generic motion
library: it is front-facing, balanced, and static, so it is stable for the
Character View and unambiguous in the field-suit's own rig.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ASSET_NAME = "caligo_field_suit"
RIG_NAME = "CaligoFieldSuit_Rig"
PROFILE_STANCE = "profile_stance"
START_FRAME = 1
END_FRAME = 48


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--render-preview", action="store_true")
    return parser.parse_args(argv)


def orient_bone(armature: bpy.types.Object, bone_name: str, direction: Vector) -> None:
    """Point a pose bone's local +Y (its length axis) at a world-space vector."""
    pose_bone = armature.pose.bones[bone_name]
    head = pose_bone.parent.tail.copy() if pose_bone.parent else pose_bone.bone.head_local.copy()
    rotation = Vector((0.0, 1.0, 0.0)).rotation_difference(direction.normalized())
    pose_bone.matrix = Matrix.Translation(head) @ rotation.to_matrix().to_4x4()
    bpy.context.view_layer.update()


def make_profile_stance(armature: bpy.types.Object) -> bpy.types.Action:
    action = bpy.data.actions.get(PROFILE_STANCE)
    if action is None:
        action = bpy.data.actions.new(PROFILE_STANCE)
    else:
        action.fcurves.clear()
    action.use_fake_user = True

    armature.animation_data_create()
    armature.animation_data.action = action
    for frame in (START_FRAME, END_FRAME):
        bpy.context.scene.frame_set(frame)
        for pose_bone in armature.pose.bones:
            pose_bone.rotation_mode = "QUATERNION"
            pose_bone.location = (0.0, 0.0, 0.0)
            pose_bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
            pose_bone.scale = (1.0, 1.0, 1.0)

        bpy.context.view_layer.update()
        # A light outward angle keeps both legs visible, while the arms hang
        # at the sides with a very slight forward opening for clean separation
        # from the torso.  These are absolute armature-space targets, which is
        # more reliable than guessed Euler axes on this imported CMU rig.
        orient_bone(armature, "LeftUpLeg", Vector((0.10, 0.0, -0.995)))
        orient_bone(armature, "RightUpLeg", Vector((-0.10, 0.0, -0.995)))
        orient_bone(armature, "LeftLeg", Vector((0.04, 0.0, -0.999)))
        orient_bone(armature, "RightLeg", Vector((-0.04, 0.0, -0.999)))
        orient_bone(armature, "LeftFoot", Vector((0.0, -1.0, 0.0)))
        orient_bone(armature, "RightFoot", Vector((0.0, -1.0, 0.0)))
        orient_bone(armature, "LeftArm", Vector((0.10, -0.04, -0.994)))
        orient_bone(armature, "RightArm", Vector((-0.10, -0.04, -0.994)))
        orient_bone(armature, "LeftForeArm", Vector((0.04, -0.07, -0.997)))
        orient_bone(armature, "RightForeArm", Vector((-0.04, -0.07, -0.997)))
        orient_bone(armature, "LeftHand", Vector((0.02, -0.22, -0.975)))
        orient_bone(armature, "RightHand", Vector((-0.02, -0.22, -0.975)))

        for pose_bone in armature.pose.bones:
            pose_bone.keyframe_insert(data_path="location", frame=frame)
            pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
            pose_bone.keyframe_insert(data_path="scale", frame=frame)

    # Blender 5 stores F-curves inside Action slots/layers, while older
    # releases expose them directly. The exporter samples the action anyway,
    # so interpolation polish is best-effort rather than a version gate.
    if hasattr(action, "fcurves"):
        for curve in action.fcurves:
            for keyframe in curve.keyframe_points:
                keyframe.interpolation = "LINEAR"
    bpy.context.scene.frame_start = START_FRAME
    bpy.context.scene.frame_end = END_FRAME
    bpy.context.scene.frame_set(START_FRAME)
    return action


def configure_preview_camera() -> None:
    """Frame the selected field suit from the direct Character View angle."""
    scene = bpy.context.scene
    camera = scene.camera
    if camera is None:
        bpy.ops.object.camera_add()
        camera = bpy.context.object
        scene.camera = camera
    camera.location = (0.0, -6.8, -0.28)
    camera.rotation_euler = (Vector((0.0, 0.0, -0.28)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 58.0
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"


def export_asset(armature: bpy.types.Object, out_dir: Path, render_preview: bool) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    configure_preview_camera()
    bpy.context.scene.render.filepath = str((out_dir / f"{ASSET_NAME}_profile_stance.png").resolve())
    bpy.ops.wm.save_as_mainfile(filepath=str((out_dir / f"{ASSET_NAME}.blend").resolve()))
    if render_preview:
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
        export_frame_step=1,
        export_force_sampling=True,
        export_skins=True,
        export_materials="EXPORT",
    )


def main() -> None:
    args = parse_args()
    armature = bpy.data.objects.get(RIG_NAME)
    if armature is None or armature.type != "ARMATURE":
        raise RuntimeError(f"Expected armature '{RIG_NAME}' in the opened source scene")
    make_profile_stance(armature)
    export_asset(armature, args.out_dir, args.render_preview)
    print(f"Generated {args.out_dir / (ASSET_NAME + '.blend')}")
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

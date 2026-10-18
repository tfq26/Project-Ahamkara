#!/usr/bin/env python3
"""Retarget the existing Charleston emote onto the MPFB operative rig.

Run from the repository root after generating ``operative_human_base.blend``::

    /Applications/Blender.app/Contents/MacOS/Blender -b \
      assets/models/operative_human_base.blend \
      -P tools/blender/retarget_operative_human_emotes.py -- \
      --out-dir assets/models --render-preview

The field-suit asset remains the motion source only.  The exported glTF
contains the MPFB human mesh and its 53-joint game rig, plus a neutral profile
stance and the retargeted in-place Charleston emote.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ASSET_NAME = "operative_human_base"
TARGET_RIG_NAME = "Ahamkara_Operative_GameRig"
TARGET_MESH_NAME = "Ahamkara_Operative_Human_Base"
SOURCE_RIG_NAME = "CaligoFieldSuit_Rig"
SOURCE_DANCE_NAME = "dance_charleston"
PROFILE_STANCE_NAME = "profile_stance"

# Names map the field-suit / CMU skeleton to the MPFB game-engine skeleton.
# Finger details deliberately remain in the MPFB bind pose for this first
# retarget pass; keeping them unkeyed avoids turning an old partial finger
# capture into broken hand deformation.
BONE_MAP = {
    "Hips": "pelvis",
    "LowerBack": "spine_01",
    "Spine": "spine_02",
    "Spine1": "spine_03",
    "Neck": "neck_01",
    "Neck1": "neck_01",
    "Head": "head",
    "LeftShoulder": "clavicle_l",
    "LeftArm": "upperarm_l",
    "LeftForeArm": "lowerarm_l",
    "LeftHand": "hand_l",
    "RightShoulder": "clavicle_r",
    "RightArm": "upperarm_r",
    "RightForeArm": "lowerarm_r",
    "RightHand": "hand_r",
    "LeftUpLeg": "thigh_l",
    "LeftLeg": "calf_l",
    "LeftFoot": "foot_l",
    "LeftToeBase": "ball_l",
    "RightUpLeg": "thigh_r",
    "RightLeg": "calf_r",
    "RightFoot": "foot_r",
    "RightToeBase": "ball_r",
}


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    parser.add_argument(
        "--motion-source",
        type=Path,
        default=Path("assets/models/caligo_field_suit.blend"),
    )
    parser.add_argument("--render-preview", action="store_true")
    return parser.parse_args(argv)


def remove_object(obj: bpy.types.Object) -> None:
    bpy.data.objects.remove(obj, do_unlink=True)


def load_source_rig(source_path: Path) -> bpy.types.Object:
    if not source_path.is_file():
        raise FileNotFoundError(f"Missing motion source: {source_path}")

    with bpy.data.libraries.load(str(source_path.resolve()), link=False) as (from_data, to_data):
        if SOURCE_RIG_NAME not in from_data.objects:
            raise RuntimeError(f"{source_path} does not contain {SOURCE_RIG_NAME}")
        to_data.objects = [SOURCE_RIG_NAME]

    source_rig = next((obj for obj in to_data.objects if obj is not None), None)
    if source_rig is None or source_rig.type != "ARMATURE":
        raise RuntimeError("Could not append the field-suit armature")
    bpy.context.collection.objects.link(source_rig)
    source_rig.name = "_MotionSource_FieldSuit"
    source_rig.hide_render = True

    source_rig.animation_data_create()
    source_action = bpy.data.actions.get(SOURCE_DANCE_NAME)
    if source_action is None:
        raise RuntimeError(f"Motion source does not contain action {SOURCE_DANCE_NAME}")
    source_rig.animation_data.action = source_action
    return source_rig


def reset_pose(armature: bpy.types.Object) -> None:
    for bone in armature.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        bone.scale = (1.0, 1.0, 1.0)


def make_profile_stance(target_rig: bpy.types.Object) -> bpy.types.Action:
    action = bpy.data.actions.get(PROFILE_STANCE_NAME)
    if action is not None:
        bpy.data.actions.remove(action, do_unlink=True)
    action = bpy.data.actions.new(PROFILE_STANCE_NAME)
    action.use_fake_user = True
    target_rig.animation_data_create()
    target_rig.animation_data.action = action

    # The MPFB file is generated in a relaxed, readable reference pose.  Two
    # bind-pose keys make that exact pose a stable engine clip.
    for frame in (1, 48):
        bpy.context.scene.frame_set(frame)
        reset_pose(target_rig)
        for bone in target_rig.pose.bones:
            bone.keyframe_insert(data_path="location", frame=frame)
            bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
            bone.keyframe_insert(data_path="scale", frame=frame)
    return action


def retarget_dance(
    source_rig: bpy.types.Object,
    target_rig: bpy.types.Object,
) -> bpy.types.Action:
    source_action = source_rig.animation_data.action
    if source_action is None:
        raise RuntimeError("Motion source has no active dance action")
    start_frame, end_frame = (round(value) for value in source_action.frame_range)
    bpy.context.scene.frame_set(start_frame)
    reset_pose(target_rig)
    target_rig.animation_data_create()
    target_rig.animation_data.action = None

    for source_name, target_name in BONE_MAP.items():
        source_bone = source_rig.pose.bones.get(source_name)
        target_bone = target_rig.pose.bones.get(target_name)
        if source_bone is None or target_bone is None:
            raise RuntimeError(f"Cannot map source {source_name} to target {target_name}")
        constraint = target_bone.constraints.new("COPY_ROTATION")
        constraint.name = f"Retarget {source_name}"
        constraint.target = source_rig
        constraint.subtarget = source_name
        constraint.target_space = "LOCAL"
        constraint.owner_space = "LOCAL"
        constraint.mix_mode = "REPLACE"

    bpy.ops.object.select_all(action="DESELECT")
    target_rig.select_set(True)
    bpy.context.view_layer.objects.active = target_rig
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.nla.bake(
        frame_start=start_frame,
        frame_end=end_frame,
        step=2,
        only_selected=False,
        visual_keying=True,
        clear_constraints=True,
        clear_parents=False,
        use_current_action=True,
        bake_types={"POSE"},
    )
    bpy.ops.object.mode_set(mode="OBJECT")
    action = target_rig.animation_data.action
    if action is None:
        raise RuntimeError("Blender did not create a retargeted action")
    # The appended field-suit source owns ``dance_charleston`` while baking.
    # Assign a temporary unique name before removing the source, then rename
    # this baked action after the old source action is gone.
    action.name = "_retargeted_dance_charleston"
    action.use_fake_user = True
    return action


def add_preview_scene(target_rig: bpy.types.Object) -> None:
    for obj in list(bpy.context.scene.objects):
        if obj.name.startswith("Preview ") or obj.name == "Preview Ground":
            remove_object(obj)
    bpy.ops.object.light_add(type="AREA", location=(3.0, -4.0, 4.8))
    key = bpy.context.object
    key.name = "Preview Key"
    key.data.energy = 900.0
    key.data.shape = "DISK"
    key.data.size = 4.0

    bpy.ops.object.light_add(type="AREA", location=(-3.5, -1.0, 2.8))
    fill = bpy.context.object
    fill.name = "Preview Fill"
    fill.data.energy = 500.0
    fill.data.color = (0.35, 0.55, 0.85)
    fill.data.size = 3.0

    bpy.ops.mesh.primitive_plane_add(size=20.0, location=(0.0, 0.0, -0.01))
    ground = bpy.context.object
    ground.name = "Preview Ground"
    material = bpy.data.materials.new("Preview Ground Material")
    material.diffuse_color = (0.045, 0.055, 0.075, 1.0)
    ground.data.materials.append(material)

    bpy.ops.object.camera_add(location=(0.0, -6.8, 1.55))
    camera = bpy.context.object
    camera.name = "Preview Camera"
    camera.rotation_euler = (Vector((0.0, 0.0, 1.15)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 54.0
    bpy.context.scene.camera = camera
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.world.color = (0.008, 0.012, 0.022)
    scene.frame_set(1)
    target_rig.animation_data.action = bpy.data.actions[PROFILE_STANCE_NAME]


def export_asset(
    target_mesh: bpy.types.Object,
    target_rig: bpy.types.Object,
    out_dir: Path,
    render_preview: bool,
) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    add_preview_scene(target_rig)
    if render_preview:
        bpy.context.scene.render.filepath = str((out_dir / f"{ASSET_NAME}_preview.png").resolve())
        bpy.ops.render.render(write_still=True)

    # The original MPFB file includes a two-metre startup cube. It is helpful
    # while authoring, but must not become a visible skinned-game asset.
    for obj in list(bpy.context.scene.objects):
        if obj.name == "Cube":
            remove_object(obj)

    bpy.ops.wm.save_as_mainfile(filepath=str((out_dir / f"{ASSET_NAME}.blend").resolve()))
    bpy.ops.object.select_all(action="DESELECT")
    target_mesh.select_set(True)
    target_rig.select_set(True)
    bpy.context.view_layer.objects.active = target_rig
    bpy.ops.export_scene.gltf(
        filepath=str((out_dir / f"{ASSET_NAME}.gltf").resolve()),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_frame_step=2,
        export_force_sampling=True,
        export_skins=True,
        export_materials="EXPORT",
    )


def main() -> None:
    args = parse_args()
    target_rig = bpy.data.objects.get(TARGET_RIG_NAME)
    target_mesh = bpy.data.objects.get(TARGET_MESH_NAME)
    if target_rig is None or target_rig.type != "ARMATURE":
        raise RuntimeError(f"Expected armature {TARGET_RIG_NAME}")
    if target_mesh is None or target_mesh.type != "MESH":
        raise RuntimeError(f"Expected mesh {TARGET_MESH_NAME}")

    source_rig = load_source_rig(args.motion_source)
    dance_action = retarget_dance(source_rig, target_rig)
    make_profile_stance(target_rig)
    remove_object(source_rig)

    # The source rig's action and any pre-existing unrelated actions are not
    # part of the new asset. Keeping only these two clips prevents accidental
    # export of legacy field-suit motions under misleading names.
    for action in list(bpy.data.actions):
        if action not in {dance_action, bpy.data.actions[PROFILE_STANCE_NAME]}:
            bpy.data.actions.remove(action, do_unlink=True)
    dance_action.name = SOURCE_DANCE_NAME

    export_asset(target_mesh, target_rig, args.out_dir, args.render_preview)
    print(f"Generated {args.out_dir / (ASSET_NAME + '.blend')}")
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

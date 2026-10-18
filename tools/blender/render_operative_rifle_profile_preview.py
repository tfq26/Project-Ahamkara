#!/usr/bin/env python3
"""Render the baked operative-and-M4A1 Character View asset for QA."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--three-quarter", action="store_true")
    parser.add_argument("--closeup", action="store_true")
    return parser.parse_args(argv)


def look_at(obj: bpy.types.Object, target: Vector) -> None:
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()


def main() -> None:
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(args.source.resolve()))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if len(meshes) < 2:
        raise RuntimeError("Expected profile mesh and the attached carbine")
    for obj in meshes:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    bpy.ops.object.light_add(type="AREA", location=(3.4, -4.0, 4.5))
    bpy.context.object.data.energy = 1000
    bpy.context.object.data.shape = "DISK"
    bpy.context.object.data.size = 4.0
    look_at(bpy.context.object, Vector((0.0, 0.0, 1.0)))
    bpy.ops.object.light_add(type="AREA", location=(-3.5, -2.0, 2.2))
    bpy.context.object.data.energy = 600
    bpy.context.object.data.size = 3.0
    look_at(bpy.context.object, Vector((0.0, 0.0, 1.0)))
    if args.closeup:
        camera_location = (1.15, -2.25, 1.45) if args.three_quarter else (0.0, -2.65, 1.4)
        camera_target = Vector((0.0, -0.05, 1.22))
    else:
        camera_location = (2.6, -5.0, 1.65) if args.three_quarter else (0.0, -5.7, 1.55)
        camera_target = Vector((0.0, 0.0, 0.95))
    bpy.ops.object.camera_add(location=camera_location)
    camera = bpy.context.object
    camera.data.lens = 55
    look_at(camera, camera_target)
    scene = bpy.context.scene
    scene.camera = camera
    scene.world = bpy.data.worlds.new("Profile QA Studio")
    scene.world.color = (0.012, 0.014, 0.020)
    engine_names = {item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engine_names else "BLENDER_EEVEE"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print(f"Rendered {args.output}")


if __name__ == "__main__":
    main()

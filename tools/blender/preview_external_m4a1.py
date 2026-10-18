#!/usr/bin/env python3
"""Render a neutral inspection image for the CC0 M4A1 source asset.

Run from the repository root:

  /Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
    --python tools/blender/preview_external_m4a1.py -- \
    --source-dir /private/tmp/ahamkara-m4a1-preview/M4A1 \
    --output /private/tmp/ahamkara-m4a1-preview.png
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def look_at(obj: bpy.types.Object, point: Vector) -> None:
    obj.rotation_euler = (point - obj.location).to_track_quat("-Z", "Y").to_euler()


def bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    return (
        Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))),
        Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points))),
    )


def material(source_dir: Path) -> bpy.types.Material:
    surface = bpy.data.materials.new("M4A1 CC0 PBR")
    surface.use_nodes = True
    nodes = surface.node_tree.nodes
    links = surface.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Metallic"].default_value = 0.68
    shader.inputs["Roughness"].default_value = 0.44
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(source_dir / "M4A1_Base_Color.png"), check_existing=True)
    links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    normal_texture = nodes.new("ShaderNodeTexImage")
    normal_texture.image = bpy.data.images.load(str(source_dir / "M4A1_Normal.png"), check_existing=True)
    normal_map = nodes.new("ShaderNodeNormalMap")
    links.new(normal_texture.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    return surface


def main() -> None:
    args = parse_args()
    source = args.source_dir / "M4A1.fbx"
    if not source.is_file():
        raise FileNotFoundError(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    weapon = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not weapon:
        raise RuntimeError("FBX import contained no mesh geometry")
    surface = material(args.source_dir)
    for obj in weapon:
        obj.data.materials.clear()
        obj.data.materials.append(surface)
    minimum, maximum = bounds(weapon)
    center = (minimum + maximum) * 0.5
    extent = maximum - minimum
    radius = max(extent.length * 0.65, 1.0)
    bpy.ops.object.light_add(type="AREA", location=center + Vector((radius, -radius, radius)))
    bpy.context.object.data.energy = 1100
    bpy.context.object.data.shape = "DISK"
    bpy.context.object.data.size = radius * 1.4
    look_at(bpy.context.object, center)
    bpy.ops.object.light_add(type="AREA", location=center + Vector((-radius, radius, radius * 0.35)))
    bpy.context.object.data.energy = 700
    bpy.context.object.data.size = radius
    look_at(bpy.context.object, center)
    bpy.ops.object.camera_add(location=center + Vector((radius * 1.35, -radius * 1.65, radius * 0.6)))
    camera = bpy.context.object
    camera.data.lens = 56
    look_at(camera, center)
    scene = bpy.context.scene
    scene.camera = camera
    render_engines = {item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in render_engines else "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    world = bpy.data.worlds.new("Inspection Studio")
    world.color = (0.012, 0.015, 0.022)
    scene.world = world
    args.output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print(f"Rendered {args.output}")


if __name__ == "__main__":
    main()

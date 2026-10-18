#!/usr/bin/env python3
"""Normalize the third-party CC0 M4A1 into Ahamkara's weapon-space contract.

The imported FBX has its muzzle along +Y and a source-specific origin.  The
engine's attachment code instead requires the firing-hand grip at (0, 0, 0)
and the muzzle along +X, all measured in metres.  This conversion is kept as a
reproducible source step; the original source remains under
``assets/source_models/m4a1_cc0``.

Run from the repository root:

  /Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
    --python tools/blender/import_cc0_m4a1_carbine.py -- --out-dir assets/models
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ASSET_NAME = "operative_carbine_tp"
SOURCE_DIR = Path("assets/source_models/m4a1_cc0")
SOURCE_RIGHT_HAND_GRIP = Vector((0.0, -0.075, -0.085))
SOURCE_LEFT_HAND_GRIP = Vector((0.0, 0.180, -0.010))
SOURCE_STOCK_SHOULDER = Vector((0.0, -0.320, -0.030))
SOURCE_MUZZLE = Vector((0.0, 0.535, 0.035))


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=SOURCE_DIR)
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    return parser.parse_args(argv)


def make_material(source_dir: Path) -> bpy.types.Material:
    """Build the source PBR material without relying on FBX material links."""
    surface = bpy.data.materials.new("Operative Carbine CC0 PBR")
    surface.use_nodes = True
    nodes = surface.node_tree.nodes
    links = surface.node_tree.links
    shader = nodes.get("Principled BSDF")
    # Ahamkara's current glTF importer consumes the material base-color factor,
    # not embedded texture maps.  Keep a readable in-engine finish while the
    # original PBR maps remain available for the renderer's texture pass.
    shader.inputs["Base Color"].default_value = (0.075, 0.09, 0.11, 1.0)
    shader.inputs["Metallic"].default_value = 0.68
    shader.inputs["Roughness"].default_value = 0.44
    base_color = nodes.new("ShaderNodeTexImage")
    base_color.image = bpy.data.images.load(str(source_dir / "M4A1_Base_Color.png"), check_existing=True)
    base_color.image.colorspace_settings.name = "sRGB"
    links.new(base_color.outputs["Color"], shader.inputs["Base Color"])
    metallic = nodes.new("ShaderNodeTexImage")
    metallic.image = bpy.data.images.load(str(source_dir / "M4A1_Metallic.png"), check_existing=True)
    metallic.image.colorspace_settings.name = "Non-Color"
    links.new(metallic.outputs["Color"], shader.inputs["Metallic"])
    roughness = nodes.new("ShaderNodeTexImage")
    roughness.image = bpy.data.images.load(str(source_dir / "M4A1_Roughness.png"), check_existing=True)
    roughness.image.colorspace_settings.name = "Non-Color"
    links.new(roughness.outputs["Color"], shader.inputs["Roughness"])
    normal = nodes.new("ShaderNodeTexImage")
    normal.image = bpy.data.images.load(str(source_dir / "M4A1_Normal.png"), check_existing=True)
    normal.image.colorspace_settings.name = "Non-Color"
    normal_map = nodes.new("ShaderNodeNormalMap")
    links.new(normal.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    return surface


def mesh_objects() -> list[bpy.types.Object]:
    return [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and obj.name != "Cube"]


def transform_to_weapon_space(objects: list[bpy.types.Object]) -> None:
    if SOURCE_LEFT_HAND_GRIP.y <= SOURCE_RIGHT_HAND_GRIP.y:
        raise RuntimeError("M4A1 source grip markers must run toward its muzzle")
    # -90° maps the FBX's +Y muzzle axis to Ahamkara's +X muzzle axis.
    conversion = Matrix.Rotation(math.radians(-90.0), 4, "Z") @ Matrix.Translation(-SOURCE_RIGHT_HAND_GRIP)
    for obj in objects:
        obj.matrix_world = conversion @ obj.matrix_world
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj.select_set(False)


def combine(objects: list[bpy.types.Object], material: bpy.types.Material) -> bpy.types.Object:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
        obj.data.materials.clear()
        obj.data.materials.append(material)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    carbine = bpy.context.object
    carbine.name = ASSET_NAME
    carbine.data.name = f"{ASSET_NAME}_mesh"
    return carbine


def add_socket(parent: bpy.types.Object, name: str, position: tuple[float, float, float]) -> None:
    bpy.ops.object.empty_add(type="ARROWS", location=position)
    socket = bpy.context.object
    socket.name = name
    socket.empty_display_size = 0.06
    socket.parent = parent


def weapon_space(source_position: Vector) -> tuple[float, float, float]:
    converted = Matrix.Rotation(math.radians(-90.0), 4, "Z") @ (source_position - SOURCE_RIGHT_HAND_GRIP)
    return tuple(round(value, 6) for value in converted)


def export(carbine: bpy.types.Object, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str((out_dir / f"{ASSET_NAME}.blend").resolve()))
    bpy.ops.object.select_all(action="DESELECT")
    carbine.select_set(True)
    bpy.context.view_layer.objects.active = carbine
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
    source = args.source_dir / "M4A1.fbx"
    if not source.is_file():
        raise FileNotFoundError(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source.resolve()))
    objects = mesh_objects()
    if len(objects) < 8:
        raise RuntimeError("Expected the M4A1 FBX to contain its component meshes")
    transform_to_weapon_space(objects)
    carbine = combine(objects, make_material(args.source_dir))
    add_socket(carbine, "weapon_root", (0.0, 0.0, 0.0))
    add_socket(carbine, "right_hand_grip", (0.0, 0.0, 0.0))
    add_socket(carbine, "left_hand_grip", weapon_space(SOURCE_LEFT_HAND_GRIP))
    add_socket(carbine, "stock_shoulder", weapon_space(SOURCE_STOCK_SHOULDER))
    add_socket(carbine, "muzzle", weapon_space(SOURCE_MUZZLE))
    export(carbine, args.out_dir)
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

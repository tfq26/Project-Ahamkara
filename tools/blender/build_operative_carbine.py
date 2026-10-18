#!/usr/bin/env python3
"""Build the canonical third-person carbine and named authoring sockets.

Unlike a first-person viewmodel, this asset has real-world scale and a stable
grip-space contract.  Its local origin is the firing-hand grip and +X runs
toward the muzzle.  Socket empties are retained in the .blend for authoring;
the matching JSON contract is consumed by tools/runtime integration.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

ASSET_NAME = "operative_carbine_tp"


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    return parser.parse_args(argv)


def make_material(name: str, color: tuple[float, float, float, float], metallic: float) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = 0.48
    return material


def add_box(
    bm: bmesh.types.BMesh,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
    material_index: int = 0,
    rotation_z: float = 0.0,
) -> None:
    face_start = len(bm.faces)
    result = bmesh.ops.create_cube(bm, size=1.0)
    verts = result["verts"]
    transform = (
        Matrix.Translation(Vector(center))
        @ Matrix.Rotation(rotation_z, 4, "Z")
        @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    )
    bmesh.ops.transform(bm, matrix=transform, verts=verts)
    for face in list(bm.faces)[face_start:]:
        face.material_index = material_index


def add_cylinder(
    bm: bmesh.types.BMesh, center: tuple[float, float, float], radius: float, length: float, material_index: int = 0
) -> None:
    face_start = len(bm.faces)
    result = bmesh.ops.create_cone(bm, segments=16, radius1=radius, radius2=radius, depth=length, cap_ends=True)
    verts = result["verts"]
    transform = Matrix.Translation(Vector(center)) @ Matrix.Rotation(math.radians(90.0), 4, "Y")
    bmesh.ops.transform(bm, matrix=transform, verts=verts)
    for face in list(bm.faces)[face_start:]:
        face.material_index = material_index


def build(out_dir: Path) -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    dark = make_material("Carbine ceramic", (0.055, 0.075, 0.09, 1.0), 0.65)
    alloy = make_material("Carbine alloy", (0.20, 0.24, 0.28, 1.0), 0.82)
    accent = make_material("Carbine signal", (0.72, 0.26, 0.06, 1.0), 0.35)
    bm = bmesh.new()
    # Origin/right grip at zero. Positive X is barrel / support-hand direction.
    add_box(bm, (-0.32, 0.00, 0.02), (0.55, 0.13, 0.16), 0)  # stock
    add_box(bm, (0.12, 0.00, 0.03), (0.44, 0.16, 0.19), 1)  # receiver
    add_box(bm, (0.54, 0.00, 0.03), (0.52, 0.13, 0.14), 0)  # handguard
    add_cylinder(bm, (0.91, 0.00, 0.035), 0.034, 0.48, 1)  # barrel
    add_cylinder(bm, (1.16, 0.00, 0.035), 0.056, 0.10, 0)  # muzzle
    add_box(bm, (0.00, -0.13, -0.08), (0.12, 0.20, 0.13), 0, math.radians(-12.0))
    add_box(bm, (0.15, -0.15, -0.10), (0.14, 0.26, 0.12), 1, math.radians(-5.0))
    add_box(bm, (0.27, 0.00, 0.16), (0.22, 0.09, 0.09), 1)  # optic
    add_box(bm, (0.27, 0.00, 0.225), (0.10, 0.035, 0.05), 2)
    add_box(bm, (0.52, -0.04, -0.09), (0.10, 0.10, 0.13), 0, math.radians(-18.0))
    mesh = bpy.data.meshes.new(ASSET_NAME + "_mesh")
    mesh.materials.append(dark)
    mesh.materials.append(alloy)
    mesh.materials.append(accent)
    bm.to_mesh(mesh)
    bm.free()
    weapon = bpy.data.objects.new(ASSET_NAME, mesh)
    bpy.context.collection.objects.link(weapon)
    for name, location in {
        "weapon_root": (0.0, 0.0, 0.0),
        "right_hand_grip": (0.0, 0.0, 0.0),
        "left_hand_grip": (0.52, 0.0, 0.0),
        "muzzle": (1.10, 0.0, 0.02),
    }.items():
        bpy.ops.object.empty_add(type="ARROWS", location=location)
        socket = bpy.context.object
        socket.name = name
        socket.empty_display_size = 0.06
        socket.parent = weapon
    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str((out_dir / f"{ASSET_NAME}.blend").resolve()))
    bpy.ops.object.select_all(action="DESELECT")
    weapon.select_set(True)
    bpy.context.view_layer.objects.active = weapon
    bpy.ops.export_scene.gltf(
        filepath=str((out_dir / f"{ASSET_NAME}.gltf").resolve()),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_yup=True,
        export_animations=False,
        export_skins=False,
        export_materials="EXPORT",
    )
    print(f"Generated {out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    build(parse_args().out_dir)

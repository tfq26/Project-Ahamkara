#!/usr/bin/env python3
"""Generate visible box meshes from a JSON level spec's collision data.

Usage:
    blender -b -P tools/blender/build_level_meshes.py -- --spec assets/levels/blood_gulch.json --out_dir assets/models
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import bmesh
    import bpy
    from mathutils import Matrix, Vector
except ImportError:
    raise RuntimeError("build_level_meshes.py must be run inside Blender")


def _box_mesh(bm, min_x, min_z, max_x, max_z, top_y, bottom_y=0.0):
    """Create a box mesh matching a collider definition. Collider uses XZ plane
    with Y as up — same as the game's coordinate system."""
    cx = (min_x + max_x) * 0.5
    cz = (min_z + max_z) * 0.5
    cy = (top_y + bottom_y) * 0.5
    sx = (max_x - min_x) * 0.5
    sy = (top_y - bottom_y) * 0.5
    sz = (max_z - min_z) * 0.5

    if sx < 0.01:
        sx = 0.01
    if sy < 0.01:
        sy = 0.01
    if sz < 0.01:
        sz = 0.01

    bmesh.ops.create_cube(
        bm, size=2.0, matrix=Matrix.Translation(Vector((cx, cy, cz))) @ Matrix.Diagonal((sx, sy, sz, 1.0)).to_4x4()
    )


def _material(r, g, b):
    mat = bpy.data.materials.new(f"mat_{r:.2f}_{g:.2f}_{b:.2f}")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.8
        bsdf.inputs["Metallic"].default_value = 0.05
    return mat


def parse_args(argv):
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("--spec", required=True)
    p.add_argument("--out_dir", default="assets/models")
    p.add_argument("--open", action="store_true")
    if "--" in argv:
        return p.parse_args(argv[argv.index("--") + 1 :])
    return p.parse_args(argv[1:])


def main(argv):
    args = parse_args(argv)
    spec = json.loads(Path(args.spec).read_text())
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)

    floor = _material(0.35, 0.38, 0.25)
    wall_mat = _material(0.28, 0.30, 0.22)
    accent = _material(0.50, 0.45, 0.30)

    bm = bmesh.new()
    colliders = spec.get("collision", [])
    for i, c in enumerate(colliders):
        mn = c["min"]
        mx = c["max"]
        top = c.get("top", c.get("top_y", 1.0))
        bottom = c.get("bottom", c.get("bottom_y", 0.0))
        is_wall = c.get("wall", False)

        start = len(bm.verts)
        _box_mesh(bm, mn[0], mn[2], mx[0], mx[2], top, bottom)

        mat_idx = 0
        if is_wall:
            mat_idx = 1
        elif i == 0:
            mat_idx = 0  # ground
        else:
            mat_idx = 2  # structures

        for face in bm.faces[start:]:
            face.material_index = mat_idx

    mesh = bpy.data.meshes.new("blood_gulch_mesh")
    mesh.materials.append(floor)
    mesh.materials.append(wall_mat)
    mesh.materials.append(accent)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("blood_gulch", mesh)
    bpy.context.collection.objects.link(obj)
    obj.location = Vector((0, 0, 0))

    base = spec.get("name", "level").lower().replace(" ", "_")
    blend_path = out_dir / f"{base}.blend"
    gltf_path = out_dir / f"{base}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=False,
        export_animations=False,
        export_materials="EXPORT",
    )

    print(f"build_level_meshes: Exported {blend_path.name} and {gltf_path.name} ({len(colliders)} colliders)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

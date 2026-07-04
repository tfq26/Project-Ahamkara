#!/usr/bin/env python3
"""Blender weapon builder — generates detailed first-person viewmodels from JSON specs.

Usage:
    blender -b -P tools/blender/build_weapon.py -- --spec tools/blender/weapons/ar15.json
    blender -b -P tools/blender/build_weapon.py -- --spec weapons/ar15.json
    blender    -P tools/blender/build_weapon.py -- --spec weapons/ar15.json --open

Spec format (see tools/blender/weapons/*.json):
{
    "name": "ar15",
    "output_name": "viewmodel_ar15",
    "scale": 1.0,
    "materials": [
        {"name": "Body",   "rgba": [0.08,0.09,0.10,1.0], "metallic": 0.90, "roughness": 0.45},
        {"name": "Grip",   "rgba": [0.06,0.06,0.07,1.0], "metallic": 0.05, "roughness": 0.80},
        {"name": "Accent", "rgba": [0.75,0.55,0.18,1.0], "metallic": 0.95, "roughness": 0.30}
    ],
    "components": [
        {"type": "box",      "pos": [x,y,z], "rot": [rx,ry,rz], "scale": [sx,sy,sz], "material": 0},
        {"type": "cylinder", "pos": [x,y,z], "rot": [rx,ry,rz], "radius": 0.02, "depth": 0.5, "verts": 16, "material": 0},
        {"type": "cone",     "pos": [x,y,z], "rot": [rx,ry,rz], "radius1": 0.05, "radius2": 0.02, "depth": 0.1, "verts": 12, "material": 0},
        {"type": "torus",    "pos": [x,y,z], "rot": [rx,ry,rz], "major_radius": 0.05, "minor_radius": 0.01, "segs": 24, "ring_segs": 8, "material": 0}
    ]
}

All position/rotation/scale values are in the canonical axis convention:
  Barrel points +X, Y-up world, glTF export_yup = True.
  Rotation is Euler XYZ in radians.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from contextlib import contextmanager
from pathlib import Path
from typing import Any

import bmesh

try:
    import bpy  # type: ignore
    from mathutils import Euler, Matrix, Vector  # type: ignore
except ImportError as exc:
    raise RuntimeError("build_weapon.py must be run inside Blender") from exc


# ── Transform helper ───────────────────────────────────────────────────────────

def _transform_matrix(
    location: tuple[float, float, float],
    rotation: tuple[float, float, float],
    scale: tuple[float, float, float],
) -> Matrix:
    return (
        Matrix.Translation(Vector(location))
        @ Euler(rotation, "XYZ").to_matrix().to_4x4()
        @ Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
    )


# ── Primitive builders ─────────────────────────────────────────────────────────

def _add_box(bm: bmesh.types.BMesh, comp: dict) -> int:
    vert_start = len(bm.verts)
    face_start = len(bm.faces)
    result = bmesh.ops.create_cube(bm, size=2.0)
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[face_start:]

    pos = comp.get("pos", (0, 0, 0))
    rot = tuple(comp.get("rot", (0, 0, 0)))
    sc = tuple(comp.get("scale", (0.1, 0.1, 0.1)))
    bmesh.ops.transform(bm, matrix=_transform_matrix(pos, rot, sc), verts=new_verts)

    mat_idx = comp.get("material", 0)
    for face in new_faces:
        face.material_index = mat_idx
    return len(new_verts)


def _add_cylinder(bm: bmesh.types.BMesh, comp: dict) -> int:
    vert_start = len(bm.verts)
    face_start = len(bm.faces)
    radius = comp.get("radius", 0.05)
    depth = comp.get("depth", 0.5)
    verts_count = comp.get("verts", 16)

    result = bmesh.ops.create_cone(
        bm,
        segments=verts_count,
        radius1=radius,
        radius2=radius,
        depth=depth,
        cap_ends=True,
    )
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[face_start:]

    pos = comp.get("pos", (0, 0, 0))
    rot = tuple(comp.get("rot", (0, 0, 0)))
    bmesh.ops.transform(bm, matrix=_transform_matrix(pos, rot, (1.0, 1.0, 1.0)), verts=new_verts)

    mat_idx = comp.get("material", 0)
    for face in new_faces:
        face.material_index = mat_idx
    return len(new_verts)


def _add_cone(bm: bmesh.types.BMesh, comp: dict) -> int:
    vert_start = len(bm.verts)
    face_start = len(bm.faces)
    r1 = comp.get("radius1", 0.08)
    r2 = comp.get("radius2", 0.02)
    depth = comp.get("depth", 0.2)
    verts_count = comp.get("verts", 12)

    result = bmesh.ops.create_cone(
        bm,
        segments=verts_count,
        radius1=r1,
        radius2=r2,
        depth=depth,
        cap_ends=True,
    )
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[face_start:]

    pos = comp.get("pos", (0, 0, 0))
    rot = tuple(comp.get("rot", (0, 0, 0)))
    bmesh.ops.transform(bm, matrix=_transform_matrix(pos, rot, (1.0, 1.0, 1.0)), verts=new_verts)

    mat_idx = comp.get("material", 0)
    for face in new_faces:
        face.material_index = mat_idx
    return len(new_verts)


def _add_torus(bm: bmesh.types.BMesh, comp: dict) -> int:
    """Add a torus via bmesh.ops.create_circle with spin extrusion."""
    major_r = comp.get("major_radius", 0.05)
    minor_r = comp.get("minor_radius", 0.01)
    segments = comp.get("segs", 24)
    ring_segs = comp.get("ring_segs", 8)

    vert_start = len(bm.verts)

    # create a circle cross-section at (major_r, 0, 0) in YZ plane, then spin
    # around Z axis
    circle_verts_start = len(bm.verts)
    bmesh.ops.create_circle(
        bm,
        segments=ring_segs,
        radius=minor_r,
        matrix=Matrix.Translation(Vector((major_r, 0.0, 0.0)))
        @ Euler((0.0, math.pi / 2, 0.0)).to_matrix().to_4x4(),
        cap_ends=False,
    )

    # spin the circle around Z into a full torus
    bmesh.ops.spin(
        bm,
        geom=[bm.verts[v] for v in range(vert_start, len(bm.verts))],
        steps=segments,
        axis=(0.0, 0.0, 1.0),
        cent=(0.0, 0.0, 0.0),
        dvec=(0.0, 0.0, 0.0),
        angle=math.pi * 2,
        use_duplicate=True,
    )

    # Remove the original circle vertices that overlap with the first spin ring
    bad_verts = [bm.verts[v] for v in range(vert_start, vert_start + ring_segs)]
    bmesh.ops.delete(bm, geom=bad_verts, context="VERTS")

    # Transform
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[:]  # crude, but works for simple models
    pos = comp.get("pos", (0, 0, 0))
    rot = tuple(comp.get("rot", (0, 0, 0)))
    bmesh.ops.transform(bm, matrix=_transform_matrix(pos, rot, (1.0, 1.0, 1.0)), verts=new_verts)

    mat_idx = comp.get("material", 0)
    for face in new_faces:
        face.material_index = mat_idx
    return len(new_verts)


_BUILDERS = {
    "box": _add_box,
    "cylinder": _add_cylinder,
    "cone": _add_cone,
    "torus": _add_torus,
}


# ── Material ───────────────────────────────────────────────────────────────────

def _make_material(name: str, rgba: tuple[float, float, float, float],
                   metallic: float = 0.12, roughness: float = 0.72):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
    return mat


# ── Build ──────────────────────────────────────────────────────────────────────

def build_from_spec(spec: dict):
    bpy.ops.wm.read_factory_settings(use_empty=True)

    materials = []
    for mat_def in spec.get("materials", []):
        rgba = tuple(mat_def["rgba"])
        mat = _make_material(
            mat_def["name"],
            rgba,
            mat_def.get("metallic", 0.12),
            mat_def.get("roughness", 0.72),
        )
        materials.append(mat)

    bm = bmesh.new()
    total_verts = 0
    for comp in spec.get("components", []):
        comp_type = comp.get("type")
        if comp_type is None:
            continue  # comment-only entries
        if comp_type not in _BUILDERS:
            print(f"WARNING: unknown component type '{comp_type}', skipping")
            continue
        total_verts += _BUILDERS[comp_type](bm, comp)

    output_name = spec.get("output_name", spec.get("name", "viewmodel_weapon"))
    mesh = bpy.data.meshes.new(f"{output_name}_mesh")
    for mat in materials:
        mesh.materials.append(mat)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(output_name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.location = Vector((0.0, 0.0, 0.0))
    obj.scale = (spec.get("scale", 1.0),) * 3

    print(f"Built '{output_name}': {total_verts} verts, {len(materials)} materials, "
          f"{len(spec.get('components',[]))} components")
    return obj


# ── Export ─────────────────────────────────────────────────────────────────────

def export_weapon(spec: dict, out_dir: Path) -> tuple[Path, Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    output_name = spec.get("output_name", spec.get("name", "viewmodel_weapon"))

    build_from_spec(spec)

    blend_path = out_dir / f"{output_name}.blend"
    gltf_path = out_dir / f"{output_name}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    # Use the standard operator path for glTF export
    original_selection = {obj.name: obj.select_get() for obj in bpy.context.scene.objects}
    for obj in bpy.context.scene.objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = bpy.context.scene.objects[0]

    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=False,
        export_animations=False,
        export_materials="EXPORT",
    )

    # Restore selection
    for obj in bpy.context.scene.objects:
        obj.select_set(original_selection.get(obj.name, False))

    print(f"Exported {blend_path.name} and {gltf_path.name} to {out_dir}")
    return blend_path, gltf_path


# ── CLI ────────────────────────────────────────────────────────────────────────

def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--spec", required=True, help="Path to weapon JSON spec")
    parser.add_argument("--out_dir", default="assets/models")
    parser.add_argument("--open", action="store_true")
    if "--" in argv:
        cli_args = argv[argv.index("--") + 1 :]
    else:
        cli_args = argv[1:]
    return parser.parse_args(cli_args)


def main(argv: list[str]) -> int:
    args = _parse_args(argv)
    spec_path = Path(args.spec)
    if not spec_path.exists():
        print(f"ERROR: spec file not found: {spec_path}")
        return 1

    spec = json.loads(spec_path.read_text())
    out_dir = Path(args.out_dir).resolve()

    blend_path, gltf_path = export_weapon(spec, out_dir)

    if args.open and not bpy.app.background:
        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

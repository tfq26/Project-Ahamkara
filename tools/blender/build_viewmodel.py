#!/usr/bin/env python3
"""Blender generator for a simple first-person rifle viewmodel.

Usage:
    blender -b -P tools/blender/build_viewmodel.py -- [--out_dir DIR]
    blender    -P tools/blender/build_viewmodel.py -- [--out_dir DIR] [--open]

This builds a small low-poly rifle from primitive meshes, joins them into one
object, saves a `.blend`, and exports glTF 2.0 (GLTF_SEPARATE) so the existing
asset importer can compile it to `.aemesh`.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path
import argparse
import bmesh
from contextlib import contextmanager

try:
    import bpy  # type: ignore
    from mathutils import Euler, Matrix, Vector  # type: ignore
except ImportError as exc:  # pragma: no cover - Blender only
    raise RuntimeError("build_viewmodel.py must be run inside Blender") from exc


class _ContextProxy:
    def __init__(self, context, active_object=None, window=None):
        self._context = context
        self.active_object = active_object
        self.window = window

    def __getattr__(self, name):
        return getattr(self._context, name)


class _WindowProxy:
    def cursor_set(self, _cursor):
        return None


class _BpyProxy:
    def __init__(self, real_bpy, context_proxy):
        self._real_bpy = real_bpy
        self.context = context_proxy

    def __getattr__(self, name):
        return getattr(self._real_bpy, name)


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


def _add_box_mesh(
    bm: bmesh.types.BMesh,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    material_index: int = 0,
) -> None:
    vert_start = len(bm.verts)
    face_start = len(bm.faces)
    result = bmesh.ops.create_cube(bm, size=2.0)
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[face_start:]
    bmesh.ops.transform(bm, matrix=_transform_matrix(location, rotation, scale), verts=new_verts)
    for face in new_faces:
        face.material_index = material_index


def _add_cylinder_mesh(
    bm: bmesh.types.BMesh,
    location: tuple[float, float, float],
    radius: float,
    depth: float,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
    vertices: int = 16,
    material_index: int = 0,
) -> None:
    vert_start = len(bm.verts)
    face_start = len(bm.faces)
    result = bmesh.ops.create_cone(
        bm,
        segments=vertices,
        radius1=radius,
        radius2=radius,
        depth=depth,
        cap_ends=True,
    )
    new_verts = bm.verts[vert_start:]
    new_faces = bm.faces[face_start:]
    bmesh.ops.transform(bm, matrix=_transform_matrix(location, rotation, (1.0, 1.0, 1.0)), verts=new_verts)
    for face in new_faces:
        face.material_index = material_index


def _material(name: str, rgba: tuple[float, float, float, float]):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Metallic"].default_value = 0.12
        bsdf.inputs["Roughness"].default_value = 0.72
    return mat


def build_rifle():
    bpy.ops.wm.read_factory_settings(use_empty=True)

    dark = _material("RifleDark", (0.10, 0.11, 0.12, 1.0))
    accent = _material("RifleAccent", (0.72, 0.55, 0.18, 1.0))

    bm = bmesh.new()
    _add_box_mesh(bm, (-0.28, 0.02, 0.0), (0.25, 0.08, 0.06))
    _add_box_mesh(bm, (0.12, 0.03, 0.0), (0.28, 0.09, 0.07))
    _add_box_mesh(bm, (0.64, 0.02, 0.0), (0.32, 0.035, 0.04))
    _add_cylinder_mesh(bm, (0.95, 0.02, 0.0), 0.025, 0.68, rotation=(0.0, math.radians(90.0), 0.0), vertices=18)
    _add_box_mesh(bm, (0.06, -0.14, 0.01), (0.045, 0.15, 0.035), rotation=(0.0, 0.0, math.radians(10.0)))
    _add_box_mesh(bm, (0.05, -0.07, -0.03), (0.035, 0.08, 0.03), rotation=(0.0, 0.0, math.radians(-10.0)))
    _add_box_mesh(bm, (0.13, -0.03, -0.045), (0.055, 0.022, 0.02))
    _add_box_mesh(bm, (0.23, 0.10, 0.0), (0.18, 0.02, 0.025))
    _add_box_mesh(bm, (0.68, 0.10, 0.0), (0.05, 0.05, 0.035))
    _add_box_mesh(bm, (0.49, 0.00, 0.0), (0.20, 0.05, 0.06))
    _add_box_mesh(bm, (0.02, -0.07, 0.045), (0.02, 0.02, 0.06), rotation=(math.radians(20.0), 0.0, math.radians(18.0)))
    _add_box_mesh(bm, (0.36, 0.06, 0.0), (0.10, 0.015, 0.022), material_index=1)

    mesh = bpy.data.meshes.new("viewmodel_rifle_mesh")
    mesh.materials.append(dark)
    mesh.materials.append(accent)
    bm.to_mesh(mesh)
    bm.free()

    rifle = bpy.data.objects.new("viewmodel_rifle", mesh)
    bpy.context.collection.objects.link(rifle)
    rifle.location = Vector((0.0, 0.0, 0.0))
    rifle.scale = (1.0, 1.0, 1.0)

    return rifle


def export_viewmodel(out_dir: Path) -> tuple[Path, Path]:
    """Build and export the rifle to `.blend` and glTF."""
    out_dir.mkdir(parents=True, exist_ok=True)
    basename = "viewmodel_rifle"

    rifle = build_rifle()
    blend_path = out_dir / f"{basename}.blend"
    gltf_path = out_dir / f"{basename}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    import io_scene_gltf2 as gltf2_addon  # type: ignore
    from io_scene_gltf2.blender.exp import export as gltf2_blender_export  # type: ignore

    proxy_context = _ContextProxy(
        bpy.context,
        active_object=None,
        window=_WindowProxy(),
    )
    proxy_bpy = _BpyProxy(bpy, proxy_context)
    original_addon_bpy = gltf2_addon.bpy
    original_export_bpy = gltf2_blender_export.bpy
    gltf2_addon.bpy = proxy_bpy
    gltf2_blender_export.bpy = proxy_bpy
    try:
        export_op = gltf2_addon.ExportGLTF2()
        export_op.filepath = str(gltf_path)
        export_op.export_format = "GLTF_SEPARATE"
        export_op.export_yup = True
        export_op.use_selection = False
        export_op.export_animations = False
        export_op.export_materials = "EXPORT"
        export_op.execute(proxy_context)
    finally:
        gltf2_addon.bpy = original_addon_bpy
        gltf2_blender_export.bpy = original_export_bpy

    print(f"build_viewmodel: wrote {blend_path.name} and {gltf_path.name} to {out_dir}")
    return blend_path, gltf_path


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--out_dir", default="assets/models")
    parser.add_argument("--open", action="store_true")
    if "--" in argv:
        cli_args = argv[argv.index("--") + 1 :]
    else:
        cli_args = argv[1:]
    return parser.parse_args(cli_args)


def main(argv: list[str]) -> int:
    args = _parse_args(argv)
    out_dir = Path(args.out_dir).resolve()

    blend_path, _ = export_viewmodel(out_dir)

    if args.open and not bpy.app.background:
        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

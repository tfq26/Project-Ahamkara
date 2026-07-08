#!/usr/bin/env python3
"""Build a full level scene in Blender from a JSON level spec.

Unlike build_level_meshes.py (which generates plain boxes from colliders),
this script produces an artist-quality scene with:
  - Ground plane with texture-ready UVs
  - Box structures with bevel modifier for softened edges
  - Cylinder/sphere rocks for organic terrain
  - Team-coloured materials (red base, blue base, neutral)
  - Base structures with proper ramps, roofs, and accent details
  - Cliff walls as large beveled boxes

Usage:
    blender -b -P tools/blender/build_map.py -- --spec assets/levels/blood_gulch.json --out_dir assets/models
    blender    -P tools/blender/build_map.py -- --spec assets/levels/blood_gulch.json --out_dir assets/models --open
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path, PurePath
from typing import Any

try:
    import bpy
    import bmesh
    from mathutils import Euler, Matrix, Vector
except ImportError:
    raise RuntimeError("build_map.py must be run inside Blender")


# ── Materials ─────────────────────────────────────────────────────────────────

def _make_material(name: str, rgba: tuple, metallic=0.05, roughness=0.80):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
    return mat


RED_TEAM    = (0.65, 0.15, 0.10, 1.0)
BLUE_TEAM   = (0.12, 0.30, 0.65, 1.0)
NEUTRAL     = (0.40, 0.42, 0.28, 1.0)
GROUND      = (0.35, 0.38, 0.25, 1.0)
WALL        = (0.25, 0.27, 0.20, 1.0)
ROCK        = (0.30, 0.28, 0.25, 1.0)
ACCENT_RED  = (0.80, 0.25, 0.15, 1.0)
ACCENT_BLUE = (0.20, 0.40, 0.75, 1.0)


# ── Primitive helpers ─────────────────────────────────────────────────────────

def _transform(loc, rot, scale):
    return (
        Matrix.Translation(Vector(loc))
        @ Euler(rot, "XYZ").to_matrix().to_4x4()
        @ Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
    )


def _add_box(name: str, bm, pos, scale, rot=(0, 0, 0), material=None, bevel_width=0.0):
    """Create a box mesh. If bevel_width > 0, apply a bevel modifier."""
    box_mesh = bpy.data.meshes.new(name)
    box_bm = bmesh.new()
    bmesh.ops.create_cube(box_bm, size=2.0,
                          matrix=_transform(pos, rot, scale))
    box_bm.to_mesh(box_mesh)
    box_bm.free()

    obj = bpy.data.objects.new(name, box_mesh)
    if material:
        obj.data.materials.append(material)
    if bevel_width > 0.01:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel_width
        mod.segments = 2
    bpy.context.collection.objects.link(obj)
    return obj


def _add_cylinder(name: str, bm_parent, pos, radius, depth, rot=(0, 0, 0), verts=16, material=None):
    """Add a cylinder (for rocks, pillars)."""
    mesh = bpy.data.meshes.new(name)
    cyl_bm = bmesh.new()
    bmesh.ops.create_cone(cyl_bm, segments=verts, radius1=radius, radius2=radius, depth=depth, cap_ends=True)
    bmesh.ops.transform(cyl_bm, matrix=_transform(pos, rot, (1, 1, 1)), verts=cyl_bm.verts[:])
    cyl_bm.to_mesh(mesh)
    cyl_bm.free()

    obj = bpy.data.objects.new(name, mesh)
    if material:
        obj.data.materials.append(material)
    bpy.context.collection.objects.link(obj)
    return obj


def _add_sphere(name: str, pos, radius, verts=16, material=None):
    """Add a sphere (boulder)."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=radius,
                               matrix=Matrix.Translation(Vector(pos)))
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    if material:
        obj.data.materials.append(material)
    bpy.context.collection.objects.link(obj)
    return obj


# ── Map building ──────────────────────────────────────────────────────────────

def _is_red(x):
    return x < -5

def _is_blue(x):
    return x > 5

def _classify(mid_x):
    if _is_red(mid_x): return "red"
    if _is_blue(mid_x): return "blue"
    return "neutral"


def build_map(spec: dict) -> list:
    """Build all map geometry as individual Blender objects. Returns list of all objects."""
    bpy.ops.wm.read_factory_settings(use_empty=True)

    red_grp   = bpy.data.collections.new("RedBase")
    blue_grp  = bpy.data.collections.new("BlueBase")
    neutral_grp = bpy.data.collections.new("Neutral")
    cliff_grp = bpy.data.collections.new("Cliffs")
    rock_grp  = bpy.data.collections.new("Rocks")
    for c in [red_grp, blue_grp, neutral_grp, cliff_grp, rock_grp]:
        bpy.context.scene.collection.children.link(c)

    mat_ground  = _make_material("Ground", GROUND, metallic=0.0, roughness=0.95)
    mat_red     = _make_material("RedBase", RED_TEAM, metallic=0.12, roughness=0.55)
    mat_blue    = _make_material("BlueBase", BLUE_TEAM, metallic=0.12, roughness=0.55)
    mat_neutral = _make_material("Neutral", NEUTRAL, metallic=0.08, roughness=0.60)
    mat_wall    = _make_material("Cliff", WALL, metallic=0.05, roughness=0.75)
    mat_rock    = _make_material("Rock", ROCK, metallic=0.02, roughness=0.90)
    mat_accent_r = _make_material("RedAccent", ACCENT_RED, metallic=0.30, roughness=0.35)
    mat_accent_b = _make_material("BlueAccent", ACCENT_BLUE, metallic=0.30, roughness=0.35)

    all_objects = []
    bm_dummy = None  # not used, just for API compat

    colliders = spec.get("collision", [])

    for i, c in enumerate(colliders):
        mn = c["min"]
        mx = c["max"]
        top = c.get("top", c.get("top_y", 1.0))
        bottom = c.get("bottom", c.get("bottom_y", 0.0))
        is_wall = c.get("wall", False)

        mid_x = (mn[0] + mx[0]) * 0.5
        mid_z = (mn[2] + mx[2]) * 0.5
        width = max(0.01, mx[0] - mn[0])
        depth = max(0.01, mx[2] - mn[2])
        height = max(0.01, top - bottom)
        cx = mid_x
        cy = (top + bottom) * 0.5
        cz = mid_z

        zone = _classify(mid_x)

        # Ground plane — flat and wide
        if height < 0.1 and not is_wall and width > 10:
            obj = _add_box(f"ground_{i}", bm_dummy,
                           (mid_x, cy, mid_z), (width * 0.5, 0.02, depth * 0.5),
                           material=mat_ground, bevel_width=0)
            neutral_grp.objects.link(obj)
            all_objects.append(obj)
            continue

        # Cliffs (tall thin walls at map edges)
        if is_wall and height > 3.0:
            obj = _add_box(f"cliff_{i}", bm_dummy,
                           (cx, cy, cz), (width * 0.5, height * 0.5, depth * 0.5),
                           material=mat_wall, bevel_width=0.03)
            cliff_grp.objects.link(obj)
            all_objects.append(obj)
            continue

        # Rocks (center terrain)
        if is_wall and height < 3.0 and abs(mid_x) < 8:
            rock_type = i % 3
            if rock_type == 0:
                obj = _add_cylinder(f"rock_{i}", bm_dummy,
                                    (cx, top * 0.6, cz),
                                    max(width, depth) * 0.4, top * 0.8,
                                    verts=12, material=mat_rock)
            elif rock_type == 1:
                obj = _add_sphere(f"rock_{i}", (cx, top * 0.5, cz),
                                  max(width, depth) * 0.35, material=mat_rock)
            else:
                obj = _add_box(f"rock_{i}", bm_dummy,
                               (cx, top * 0.5, cz),
                               (width * 0.35, top * 0.25, depth * 0.35),
                               rot=(0, i * 0.5, 0), material=mat_rock,
                               bevel_width=0.15)
            rock_grp.objects.link(obj)
            all_objects.append(obj)
            continue

        # Structures (bases, ramps, cover)
        if zone == "red":
            mat = mat_red
            accent_mat = mat_accent_r
            grp = red_grp
        elif zone == "blue":
            mat = mat_blue
            accent_mat = mat_accent_b
            grp = blue_grp
        else:
            mat = mat_neutral
            accent_mat = None
            grp = neutral_grp

        # Main structure
        obj = _add_box(f"struct_{i}", bm_dummy,
                       (cx, cy, cz), (width * 0.5, height * 0.5, depth * 0.5),
                       material=mat, bevel_width=0.02)
        grp.objects.link(obj)
        all_objects.append(obj)

        # Add accent strips on top of walls
        if accent_mat and height > 1.0 and width > 1.0:
            obj2 = _add_box(f"accent_{i}", bm_dummy,
                            (cx, top - 0.03, cz),
                            (width * 0.48, 0.03, depth * 0.48),
                            material=accent_mat, bevel_width=0)
            grp.objects.link(obj2)
            all_objects.append(obj2)

    base_name = Path(spec.get("name", "level")).stem.lower().replace(" ", "_")
    print(f"Built map: {len(all_objects)} objects across 5 collections "
          f"(red={len(red_grp.objects)}, blue={len(blue_grp.objects)}, "
          f"neutral={len(neutral_grp.objects)}, cliffs={len(cliff_grp.objects)}, "
          f"rocks={len(rock_grp.objects)})")
    return all_objects


# ── Export ────────────────────────────────────────────────────────────────────

def export_scene(spec: dict, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    base_name = Path(spec.get("name", "level")).stem.lower().replace(" ", "_")

    build_map(spec)

    # Join all objects into one for efficient rendering
    bpy.ops.object.select_all(action="SELECT")
    if bpy.context.selected_objects:
        bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]
        bpy.ops.object.join()

    blend_path = out_dir / f"{base_name}.blend"
    gltf_path = out_dir / f"{base_name}.gltf"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    bpy.ops.export_scene.gltf(
        filepath=str(gltf_path),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=False,
        export_materials="EXPORT",
    )

    print(f"Exported {blend_path.name} and {gltf_path.name}")
    return blend_path, gltf_path


# ── CLI ───────────────────────────────────────────────────────────────────────

def _parse_args(argv):
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("--spec", required=True)
    p.add_argument("--out_dir", default="assets/models")
    p.add_argument("--open", action="store_true")
    if "--" in argv:
        return p.parse_args(argv[argv.index("--") + 1:])
    return p.parse_args(argv[1:])


def main(argv):
    args = _parse_args(argv)
    spec_path = Path(args.spec)
    if not spec_path.exists():
        print(f"ERROR: spec not found: {spec_path}")
        return 1
    spec = json.loads(spec_path.read_text())
    out_dir = Path(args.out_dir).resolve()

    blend_path, gltf_path = export_scene(spec, out_dir)

    if args.open and not bpy.app.background:
        bpy.ops.wm.open_mainfile(filepath=str(blend_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python3
"""Generate the first Caligo art blockout and its Blender source scenes.

The generator deliberately keeps the source deterministic and low-tech: the
engine's current glTF importer consumes static meshes and base-color factors,
so the first pass focuses on silhouette, scale, landmarks and LOD structure.
The production material pass can replace these materials without changing the
scene layout or asset names.

Usage:
    blender -b -P tools/blender/build_caligo.py -- --out_dir assets/models
    python3 tools/blender/build_caligo.py --out_dir assets/models

Outputs:
    caligo_director.blend/.gltf           exterior globe and landmarks
    caligo_director_lod1.blend/.gltf     reduced Director representation
    caligo_director_lod2.blend/.gltf     distant Director representation
    caligo_cutaway.blend/.gltf           inspectable Crown cutaway blockout
    caligo_surface_ruins.blend/.gltf     500m research-campus blockout
"""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path

try:
    import bpy  # type: ignore
    from mathutils import Vector  # type: ignore
except ImportError:  # pragma: no cover - imported by pure tests
    bpy = None
    Vector = None


COLORS = {
    "planet": (0.27, 0.14, 0.08, 1.0),
    "sand": (0.55, 0.30, 0.12, 1.0),
    "rock": (0.10, 0.08, 0.07, 1.0),
    "ruin": (0.22, 0.20, 0.17, 1.0),
    "concrete": (0.30, 0.29, 0.26, 1.0),
    "steel": (0.10, 0.14, 0.16, 1.0),
    "warning": (0.78, 0.30, 0.05, 1.0),
    "habitat": (0.12, 0.28, 0.30, 1.0),
    "green": (0.10, 0.22, 0.12, 1.0),
    "electric": (0.16, 0.55, 0.70, 1.0),
    "white": (0.70, 0.68, 0.60, 1.0),
}


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out_dir", default="assets/models")
    if "--" in argv:
        return parser.parse_args(argv[argv.index("--") + 1 :])
    return parser.parse_args(argv[1:])


def object_count_for_lod(lod: int) -> tuple[int, int]:
    """Return (planet segments, landmark count) for a Director LOD."""
    return {0: (32, 12), 1: (16, 6), 2: (8, 2)}.get(lod, (16, 6))


def _require_blender() -> None:
    if bpy is None:
        raise RuntimeError("This generator must run inside Blender")


def reset_scene() -> None:
    _require_blender()
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name: str, color_name: str):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = COLORS[color_name]
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = COLORS[color_name]
        bsdf.inputs["Roughness"].default_value = 0.82
        if color_name in ("electric", "warning", "habitat"):
            bsdf.inputs["Emission Color"].default_value = COLORS[color_name]
            bsdf.inputs["Emission Strength"].default_value = 1.8
    return mat


def apply_object(obj, name: str, mat) -> None:
    obj.name = name
    if obj.data and hasattr(obj.data, "materials"):
        obj.data.materials.append(mat)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    obj.select_set(False)


def blender_location(location: tuple[float, float, float]) -> tuple[float, float, float]:
    """Map the generator's Y-up game coordinates into Blender's Z-up space."""
    x, y, z = location
    return (x, -z, y)


def blender_size(size: tuple[float, float, float]) -> tuple[float, float, float]:
    """Map a Y-up game-space extent into Blender's object axes."""
    x, y, z = size
    return (x, z, y)


def cube(name: str, location: tuple[float, float, float], scale: tuple[float, float, float], mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_location(location))
    obj = bpy.context.object
    obj.scale = blender_size(scale)
    apply_object(obj, name, mat)
    return obj


def cylinder(name: str, location: tuple[float, float, float], radius: float, depth: float, mat, vertices: int = 16):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth, location=blender_location(location)
    )
    obj = bpy.context.object
    apply_object(obj, name, mat)
    return obj


def sphere(name: str, location: tuple[float, float, float], radius: float, mat, segments: int = 24, rings: int = 12):
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=segments, ring_count=rings, radius=radius, location=blender_location(location)
    )
    obj = bpy.context.object
    apply_object(obj, name, mat)
    return obj


def torus(name: str, location: tuple[float, float, float], major: float, minor: float, mat, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major,
        minor_radius=minor,
        major_segments=32,
        minor_segments=8,
        location=blender_location(location),
        rotation=rotation,
    )
    obj = bpy.context.object
    apply_object(obj, name, mat)
    return obj


def export_scene(out_dir: Path, name: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out_dir / f"{name}.blend"))
    bpy.ops.export_scene.gltf(
        filepath=str(out_dir / f"{name}.gltf"),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=False,
        export_animations=False,
        export_materials="EXPORT",
    )


# ---------------------------------------------------------------------------
# Pure-Python glTF fallback
# ---------------------------------------------------------------------------


def _fallback_mesh() -> dict:
    return {"primitives": [], "positions": [], "normals": [], "indices": [], "materials": []}


def _fallback_material(scene: dict, color: tuple[float, float, float, float]) -> int:
    key = tuple(round(v, 5) for v in color)
    for index, existing in enumerate(scene["materials"]):
        if existing == key:
            return index
    scene["materials"].append(key)
    return len(scene["materials"]) - 1


def _fallback_primitive(
    scene: dict,
    positions: list[tuple[float, float, float]],
    normals: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
    color: tuple[float, float, float, float],
) -> None:
    base = len(scene["positions"])
    scene["positions"].extend(positions)
    scene["normals"].extend(normals)
    scene["indices"].extend((base + a, base + b, base + c) for a, b, c in triangles)
    scene["primitives"].append(
        {
            "first_vertex": base,
            "vertex_count": len(positions),
            "first_index": len(scene["indices"]) - len(triangles),
            "index_count": len(triangles) * 3,
            "material": _fallback_material(scene, color),
        }
    )


def _fallback_cube(
    scene: dict,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
    color: tuple[float, float, float, float],
) -> None:
    cx, cy, cz = center
    sx, sy, sz = (v * 0.5 for v in size)
    faces = [
        ((0, 0, 1), [(-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz)]),
        ((0, 0, -1), [(sx, -sy, -sz), (-sx, -sy, -sz), (-sx, sy, -sz), (sx, sy, -sz)]),
        ((1, 0, 0), [(sx, -sy, sz), (sx, -sy, -sz), (sx, sy, -sz), (sx, sy, sz)]),
        ((-1, 0, 0), [(-sx, -sy, -sz), (-sx, -sy, sz), (-sx, sy, sz), (-sx, sy, -sz)]),
        ((0, 1, 0), [(-sx, sy, sz), (sx, sy, sz), (sx, sy, -sz), (-sx, sy, -sz)]),
        ((0, -1, 0), [(-sx, -sy, -sz), (sx, -sy, -sz), (sx, -sy, sz), (-sx, -sy, sz)]),
    ]
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    for normal, corners in faces:
        start = len(positions)
        positions.extend((cx + x, cy + y, cz + z) for x, y, z in corners)
        normals.extend([normal] * 4)
        triangles.extend(((start, start + 1, start + 2), (start, start + 2, start + 3)))
    _fallback_primitive(scene, positions, normals, triangles, color)


def _fallback_sphere(
    scene: dict,
    center: tuple[float, float, float],
    radius: float,
    color: tuple[float, float, float, float],
    segments: int,
    rings: int,
) -> None:
    cx, cy, cz = center
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    for ring in range(rings + 1):
        phi = math.pi * ring / rings
        for segment in range(segments):
            theta = 2.0 * math.pi * segment / segments
            normal = (math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta))
            normals.append(normal)
            positions.append((cx + radius * normal[0], cy + radius * normal[1], cz + radius * normal[2]))
    for ring in range(rings):
        for segment in range(segments):
            a = ring * segments + segment
            b = ring * segments + (segment + 1) % segments
            c = (ring + 1) * segments + (segment + 1) % segments
            d = (ring + 1) * segments + segment
            triangles.extend(((a, b, c), (a, c, d)))
    _fallback_primitive(scene, positions, normals, triangles, color)


def _fallback_torus(
    scene: dict,
    center: tuple[float, float, float],
    major: float,
    minor: float,
    color: tuple[float, float, float, float],
) -> None:
    cx, cy, cz = center
    segments, rings = 32, 8
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    for segment in range(segments):
        theta = 2.0 * math.pi * segment / segments
        for ring in range(rings):
            phi = 2.0 * math.pi * ring / rings
            radial = major + minor * math.cos(phi)
            normal = (math.cos(theta) * math.cos(phi), math.sin(phi), math.sin(theta) * math.cos(phi))
            positions.append((cx + radial * math.cos(theta), cy + minor * math.sin(phi), cz + radial * math.sin(theta)))
            normals.append(normal)
    for segment in range(segments):
        for ring in range(rings):
            a = segment * rings + ring
            b = ((segment + 1) % segments) * rings + ring
            c = ((segment + 1) % segments) * rings + (ring + 1) % rings
            d = segment * rings + (ring + 1) % rings
            triangles.extend(((a, b, c), (a, c, d)))
    _fallback_primitive(scene, positions, normals, triangles, color)


def _write_fallback_gltf(out_dir: Path, name: str, scene: dict) -> None:
    raw = bytearray()
    views = []
    accessors = []

    def append_bytes(data: bytes, target: int, component_type: int, count: int, typ: str) -> None:
        while len(raw) % 4:
            raw.append(0)
        offset = len(raw)
        raw.extend(data)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        accessor = {"bufferView": len(views) - 1, "componentType": component_type, "count": count, "type": typ}
        accessors.append(accessor)

    flat_positions = [value for point in scene["positions"] for value in point]
    flat_normals = [value for point in scene["normals"] for value in point]
    flat_indices = [value for tri in scene["indices"] for value in tri]
    append_bytes(struct.pack(f"<{len(flat_positions)}f", *flat_positions), 34962, 5126, len(scene["positions"]), "VEC3")
    position_accessor = len(accessors) - 1
    append_bytes(struct.pack(f"<{len(flat_normals)}f", *flat_normals), 34962, 5126, len(scene["normals"]), "VEC3")
    normal_accessor = len(accessors) - 1
    append_bytes(struct.pack(f"<{len(flat_indices)}I", *flat_indices), 34963, 5125, len(flat_indices), "SCALAR")
    index_accessor = len(accessors) - 1

    # The current runtime flattens glTF primitives into one compiled model and
    # does not retain primitive-local index ranges. Keep the fallback as one
    # primitive so the importer cannot draw the complete index buffer repeatedly.
    primitives = [
        {
            "attributes": {"POSITION": position_accessor, "NORMAL": normal_accessor},
            "indices": index_accessor,
            "material": 0,
            "mode": 4,
        }
    ]
    document = {
        "asset": {"version": "2.0", "generator": "Ahamkara Caligo fallback generator"},
        "buffers": [{"uri": name + ".bin", "byteLength": len(raw)}],
        "bufferViews": views,
        "accessors": accessors,
        "materials": [
            {"pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": 0.0, "roughnessFactor": 0.82}}
            for color in scene["materials"]
        ],
        "meshes": [{"primitives": primitives}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / (name + ".bin")).write_bytes(raw)
    (out_dir / (name + ".gltf")).write_text(json.dumps(document, separators=(",", ":")))


def _fallback_scene(name: str, lod: int = 0) -> dict:
    scene = _fallback_mesh()
    if name.startswith("caligo_director"):
        segments, landmarks = object_count_for_lod(lod)
        _fallback_sphere(scene, (0, 0, 0), 5.0, COLORS["planet"], segments, max(6, segments // 2))
        for i in range(max(1, landmarks // 3)):
            _fallback_torus(scene, (0, 0, 0), 5.04 + i * 0.025, 0.035, COLORS["sand"])
        if lod <= 1:
            for i in range(landmarks):
                angle = 2.0 * math.pi * i / landmarks
                _fallback_cube(
                    scene,
                    (math.cos(angle) * 5.02, math.sin(angle * 2.0) * 0.7, math.sin(angle) * 5.02),
                    (0.36, 0.84, 0.16),
                    COLORS["ruin"],
                )
            for angle in (0.35, 2.4, 4.7):
                _fallback_torus(
                    scene,
                    (math.cos(angle) * 5.08, math.sin(angle) * 0.7, math.sin(angle) * 5.08),
                    0.42,
                    0.055,
                    COLORS["habitat"],
                )
        return scene
    if name == "caligo_cutaway":
        _fallback_cube(scene, (0, 0, -1.6), (11, 8.8, 0.7), COLORS["rock"])
        _fallback_cube(scene, (-5, 0, 0), (0.7, 8.8, 4), COLORS["rock"])
        _fallback_cube(scene, (5, 0, 0), (0.7, 8.8, 4), COLORS["rock"])
        _fallback_cube(scene, (0, 3.5, 0), (8.4, 0.7, 3), COLORS["concrete"])
        _fallback_cube(scene, (0, 1.1, 0), (8.4, 0.5, 3), COLORS["steel"])
        _fallback_cube(scene, (0, -1.1, 0), (8.4, 0.5, 3), COLORS["green"])
        _fallback_sphere(scene, (0, 0, 0), 0.58, COLORS["warning"], 24, 12)
        return scene
    _fallback_cube(scene, (0, -0.35, 0), (500, 0.7, 500), COLORS["sand"])
    for x, z, w, d, h in (
        (-70, -20, 38, 32, 34),
        (40, -10, 45, 38, 26),
        (0, 85, 42, 35, 22),
        (-170, -130, 18, 18, 58),
        (145, -145, 24, 18, 64),
        (150, 145, 26, 20, 72),
    ):
        _fallback_cube(scene, (x, h * 0.5, z), (w, h, d), COLORS["concrete"])
    _fallback_cube(scene, (0, 0.2, -115), (340, 0.5, 8), COLORS["steel"])
    for x in (-190, -150, -110, 110, 150, 190):
        _fallback_cube(scene, (x, 12, 145), (4.4, 24, 4.4), COLORS["warning"])
    return scene


def build_director(out_dir: Path, lod: int = 0) -> None:
    reset_scene()
    planet_mat = material("Caligo_Exterior", "planet")
    sand_mat = material("Caligo_DuneBands", "sand")
    ruin_mat = material("Caligo_Buried_Ruins", "ruin")
    electric_mat = material("Caligo_StormGlow", "electric")
    habitat_mat = material("Caligo_Crown", "habitat")

    segments, landmark_count = object_count_for_lod(lod)
    radius = 5.0
    sphere(f"caligo_planet_lod{lod}", (0, 0, 0), radius, planet_mat, segments, max(6, segments // 2))

    # Broad dune bands are intentionally geometric in the blockout. The final
    # material pass will replace them with displacement/normal detail.
    for i in range(max(1, landmark_count // 3)):
        angle = (2.0 * math.pi * i) / max(1, landmark_count // 3)
        torus(
            f"caligo_dune_band_{i:02d}",
            (0, 0, 0),
            radius + 0.04 + i * 0.025,
            0.035,
            sand_mat,
            rotation=(0.0, math.radians(8.0 + i * 4.0), angle),
        )

    if lod <= 1:
        # Buried colonial silhouettes and three concealed Habitat Crowns.
        for i in range(landmark_count):
            angle = (2.0 * math.pi * i) / landmark_count
            x = math.cos(angle) * (radius + 0.02)
            z = math.sin(angle) * (radius + 0.02)
            y = math.sin(angle * 2.0) * 0.7
            cube(f"buried_colony_shell_{i:02d}", (x, y, z), (0.18, 0.42, 0.08), ruin_mat)

        for i, angle in enumerate((0.35, 2.4, 4.7)):
            x = math.cos(angle) * (radius + 0.08)
            z = math.sin(angle) * (radius + 0.08)
            torus(
                f"habitat_crown_{i:02d}",
                (x, math.sin(angle) * 0.7, z),
                0.42,
                0.055,
                habitat_mat,
                rotation=(math.radians(65), 0, angle),
            )

        # A visible but damaged mass-driver silhouette.
        cube("orbital_mass_driver", (0.0, -0.4, radius + 0.15), (1.5, 0.08, 0.06), ruin_mat)
        torus(
            "mass_driver_field",
            (0.0, -0.25, radius + 0.2),
            0.42,
            0.025,
            electric_mat,
            rotation=(math.radians(90), 0, 0),
        )

    export_scene(out_dir, "caligo_director" if lod == 0 else f"caligo_director_lod{lod}")


def build_cutaway(out_dir: Path) -> None:
    reset_scene()
    rock_mat = material("Cutaway_Crust", "rock")
    concrete_mat = material("Cutaway_Habitat", "concrete")
    steel_mat = material("Cutaway_Structure", "steel")
    electric_mat = material("Cutaway_Power", "electric")
    habitat_mat = material("Cutaway_Living_Rim", "green")
    sun_mat = material("Cutaway_Artificial_Sun", "warning")

    # The section is intentionally open toward the camera. It reads as a
    # technical cutaway in the Director and remains useful as an art blockout.
    cube("cutaway_crust_back", (0, 0, -1.6), (5.5, 4.4, 0.35), rock_mat)
    cube("cutaway_crust_left", (-5.0, 0, 0), (0.35, 4.4, 2.0), rock_mat)
    cube("cutaway_crust_right", (5.0, 0, 0), (0.35, 4.4, 2.0), rock_mat)
    cube("crown_surface", (0, 3.5, 0), (4.2, 0.35, 1.5), concrete_mat)
    cube("underworks_floor", (0, 1.1, 0), (4.2, 0.25, 1.5), steel_mat)
    cube("inner_rim_floor", (0, -1.1, 0), (4.2, 0.25, 1.5), habitat_mat)
    cylinder("artificial_sun_spine", (0, 0.0, 0), 0.16, 4.4, steel_mat, 12)
    sphere("artificial_sun", (0, 0.0, 0), 0.58, sun_mat, 24, 12)
    for x in (-3.0, -1.5, 1.5, 3.0):
        cylinder(f"habitat_support_{str(x).replace('-', 'm')}", (x, 0.0, -0.7), 0.09, 4.0, electric_mat, 8)
    export_scene(out_dir, "caligo_cutaway")


def add_building(name: str, x: float, z: float, width: float, depth: float, height: float, mat, damaged: bool = False):
    cube(name + "_shell", (x, height * 0.5, z), (width, height, depth), mat)
    # Vertical voids and structural ribs make the blockout read as a ruin even
    # before detailed interiors are authored.
    if damaged:
        cube(
            name + "_collapse",
            (x + width * 0.35, height * 0.72, z),
            (width * 0.28, height * 0.18, depth * 1.1),
            material(name + "_collapse_mat", "rock"),
        )
    for i in range(3):
        px = x - width * 0.65 + (i * width * 0.65)
        cube(
            f"{name}_rib_{i}",
            (px, height * 0.5, z - depth * 1.02),
            (0.06, height, 0.07),
            material(f"{name}_rib_mat_{i}", "steel"),
        )


def build_surface_ruins(out_dir: Path) -> None:
    reset_scene()
    sand_mat = material("Surface_Dune", "sand")
    rock_mat = material("Surface_Rock", "rock")
    concrete_mat = material("Colonial_Concrete", "concrete")
    steel_mat = material("Colonial_Steel", "steel")
    warning_mat = material("Power_Warning", "warning")
    electric_mat = material("Storm_Electric", "electric")
    ecology_mat = material("Ecology_Remnant", "green")

    # 500m blockout ground and dune ridges.
    cube("surface_terrain", (0, -0.35, 0), (500, 0.35, 500), sand_mat)
    for i, (x, z, w, d, h) in enumerate(
        (
            (-170, -80, 90, 22, 14),
            (-90, 110, 120, 28, 18),
            (125, 80, 105, 25, 15),
            (170, -110, 80, 18, 12),
            (0, 170, 160, 22, 20),
        )
    ):
        cube(f"dune_ridge_{i:02d}", (x, h * 0.5 - 0.3, z), (w, h, d), sand_mat)

    # The playable slice is deliberately denser than the distant skyline: a
    # recognisable concourse, cover and route markers give the player a real
    # first-person destination as soon as Surface Ruins launches.
    cube("central_concourse", (-12, 0.15, 25), (124, 0.30, 34), concrete_mat)
    cube("concourse_spine", (-12, 0.35, 25), (4, 0.35, 142), steel_mat)
    for i, (x, z, width, depth, height) in enumerate(
        (
            (-43, 4, 8, 4, 2.4),
            (-28, 43, 6, 5, 2.0),
            (6, 7, 9, 4, 2.6),
            (25, 43, 7, 5, 2.2),
            (52, 21, 8, 4, 2.4),
            (-5, 53, 5, 8, 2.0),
        )
    ):
        cube(f"concourse_cover_{i:02d}", (x, height * 0.5, z), (width, height, depth), steel_mat)
    for i, (x, z) in enumerate(((-75, 18), (-10, -15), (39, 30), (-2, 114))):
        cylinder(f"route_beacon_{i:02d}", (x, 3.0, z), 0.55, 6.0, warning_mat, 12)
        sphere(f"route_beacon_light_{i:02d}", (x, 6.3, z), 0.85, electric_mat, 16, 8)

    # Three hero research buildings.
    add_building("planetary_sciences_admin", -70, -20, 38, 32, 34, concrete_mat, True)
    add_building("magnetodynamic_research_hall", 40, -10, 45, 38, 26, steel_mat, False)
    add_building("electrorganic_ecology_institute", 0, 85, 42, 35, 22, ecology_mat, True)

    # Skyline and campus shells. These are intentionally numerous but cheap.
    skyline = [
        (-170, -130, 18, 18, 58),
        (-130, -145, 14, 20, 42),
        (-20, -145, 22, 18, 70),
        (75, -135, 18, 18, 50),
        (145, -145, 24, 18, 64),
        (-175, 30, 20, 22, 46),
        (135, 30, 18, 24, 54),
        (-145, 135, 20, 18, 38),
        (150, 145, 26, 20, 72),
    ]
    for i, (x, z, w, d, h) in enumerate(skyline):
        add_building(f"colonial_skyline_{i:02d}", x, z, w, d, h, concrete_mat, i % 3 == 0)

    # Buried transit, power and frontier infrastructure.
    cube("buried_maglev_track", (0, 0.2, -115), (170, 0.25, 4), steel_mat)
    for i, x in enumerate(range(-150, 151, 50)):
        cube(f"maglev_pylon_{i:02d}", (x, 4.0, -115), (1.2, 4.0, 1.2), steel_mat)
    for i, x in enumerate((-190, -150, -110, 110, 150, 190)):
        cylinder(f"storm_grounding_tower_{i:02d}", (x, 12, 145), 2.2, 24, warning_mat, 12)
        torus(f"grounding_ring_{i:02d}", (x, 24, 145), 2.8, 0.25, electric_mat)
    for i, (x, z) in enumerate(((-205, 80), (-185, 105), (-150, 75), (165, 85), (200, 105))):
        cube(f"frontier_shelter_{i:02d}", (x, 3, z), (12, 3, 9), rock_mat)
    for i in range(5):
        cylinder(
            f"petrified_electrorganic_tree_{i:02d}", (-120 + i * 18, 8, 160 - (i % 2) * 15), 2.5, 16, ecology_mat, 8
        )

    export_scene(out_dir, "caligo_surface_ruins")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    out_dir = Path(args.out_dir)
    if bpy is None:
        for name, lod in (("caligo_director", 0), ("caligo_director_lod1", 1), ("caligo_director_lod2", 2)):
            _write_fallback_gltf(out_dir, name, _fallback_scene(name, lod))
        _write_fallback_gltf(out_dir, "caligo_cutaway", _fallback_scene("caligo_cutaway"))
        _write_fallback_gltf(out_dir, "caligo_surface_ruins", _fallback_scene("caligo_surface_ruins"))
        print("build_caligo: Blender unavailable; generated deterministic glTF blockout fallback")
        return 0
    build_director(out_dir, 0)
    build_director(out_dir, 1)
    build_director(out_dir, 2)
    build_cutaway(out_dir)
    build_surface_ruins(out_dir)
    print("build_caligo: generated Director, cutaway and Surface Ruins scenes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(__import__("sys").argv))

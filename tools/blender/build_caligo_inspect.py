#!/usr/bin/env python3
"""Build an inspectable, presentation-ready Caligo Director Blender scene.

This is intentionally separate from the engine export blockout.  The resulting
blend is an art-direction file: it contains named collections, a camera, lights,
materials, a storm shell, landmarks, and a render-ready 3/4 planet view.

Usage:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_caligo_inspect.py
"""

from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "models"
RADIUS = 5.0


def collection(name: str):
    col = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if col.name not in bpy.context.scene.collection.children:
        try:
            bpy.context.scene.collection.children.link(col)
        except RuntimeError:
            pass
    return col


def move_to(obj, col):
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    col.objects.link(obj)
    return obj


def mat_principled(name, base, roughness=0.7, metallic=0.0, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*base, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def planet_material():
    mat = bpy.data.materials.new("Caligo / storm desert surface")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    tex = nodes.new("ShaderNodeTexNoise")
    tex.inputs["Scale"].default_value = 2.8
    tex.inputs["Detail"].default_value = 6.0
    tex.inputs["Roughness"].default_value = 0.72
    tex.inputs["Distortion"].default_value = 0.28
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.20
    ramp.color_ramp.elements[0].color = (0.025, 0.006, 0.003, 1.0)
    ramp.color_ramp.elements[1].position = 0.82
    ramp.color_ramp.elements[1].color = (0.72, 0.24, 0.035, 1.0)
    low = ramp.color_ramp.elements.new(0.38)
    low.color = (0.16, 0.025, 0.008, 1.0)
    mid = ramp.color_ramp.elements.new(0.58)
    mid.color = (0.42, 0.095, 0.012, 1.0)
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.22
    bump.inputs["Distance"].default_value = 0.12
    links.new(tex.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(tex.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 0.08
    bsdf.inputs["Roughness"].default_value = 0.86
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def emission_mat(name, color, strength):
    return mat_principled(name, color, 0.35, 0.1, color, strength)


def orient_to_normal(obj, normal):
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = Vector((0.0, 0.0, 1.0)).rotation_difference(Vector(normal).normalized())


def surface_point(lat_deg, lon_deg, radius=RADIUS):
    lat = math.radians(lat_deg)
    lon = math.radians(lon_deg)
    normal = Vector((math.cos(lat) * math.cos(lon), math.sin(lat), math.cos(lat) * math.sin(lon)))
    return normal * radius, normal


def add_uv_sphere(name, loc, scale, mat, col, segments=128, rings=64):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1.0, location=loc)
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.scale = scale
    if mat:
        obj.data.materials.append(mat)
    bpy.ops.object.shade_smooth()
    return obj


def add_beveled_cube(name, loc, scale, mat, col, bevel=0.05):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        modifier = obj.modifiers.new("weathered edges", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
    return obj


def add_torus(name, loc, major, minor, mat, col, normal=None):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major, minor_radius=minor, major_segments=64, minor_segments=12, location=loc
    )
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.data.materials.append(mat)
    if normal is not None:
        orient_to_normal(obj, normal)
    return obj


def add_curve(name, points, mat, col, bevel=0.025):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = bevel
    curve.bevel_resolution = 3
    spline = curve.splines.new("BEZIER")
    spline.bezier_points.add(len(points) - 1)
    for bp, point in zip(spline.bezier_points, points):
        bp.co = point
        bp.handle_left_type = "AUTO"
        bp.handle_right_type = "AUTO"
    obj = bpy.data.objects.new(name, curve)
    col.objects.link(obj)
    obj.data.materials.append(mat)
    return obj


def add_landmark(name, lat, lon, height, width, mat, col):
    point, normal = surface_point(lat, lon, RADIUS + height * 0.5)
    obj = add_beveled_cube(name, point, (width, width, height), mat, col, bevel=0.04)
    orient_to_normal(obj, normal)
    return obj


def create_planet(cols, mats):
    planet = add_uv_sphere(
        "CALIGO / displaced storm desert planet",
        (0, 0, 0),
        (RADIUS, RADIUS, RADIUS),
        mats["planet"],
        cols["00_PLANET"],
        160,
        96,
    )
    # Real geometric relief keeps the .blend inspectable even without textures.
    for vertex in planet.data.vertices:
        p = vertex.co.normalized()
        ridge = 0.045 * math.sin(p.x * 17.0 + p.z * 8.0) + 0.028 * math.sin(p.y * 31.0 - p.x * 11.0)
        basin = -0.055 * max(0.0, math.sin(p.x * 3.0 + p.z * 4.0)) ** 8
        vertex.co = p * (1.0 + ridge + basin)
    planet.data.update()

    # Vitrified scar bands and a few bright storm fronts.
    for i, lon in enumerate((-48, -12, 26, 64)):
        point, normal = surface_point(4 + i * 3, lon, RADIUS + 0.025)
        add_torus(
            f"vitrified electrical scar {i + 1:02d}",
            point,
            1.12 + i * 0.19,
            0.025,
            mats["scar"],
            cols["01_STORM_SHELLS"],
            normal,
        )
    for i, lon in enumerate((-35, 18, 72)):
        points = []
        for step in range(18):
            lat = -18 + step * 2.2 + i * 5
            point, normal = surface_point(lat, lon + math.sin(step * 0.7) * 2.5, RADIUS + 0.07)
            points.append(point)
        add_curve(f"magnetic storm front {i + 1:02d}", points, mats["storm"], cols["01_STORM_SHELLS"], 0.035)

    return planet


def create_landmarks(cols, mats):
    # Deliberately mix corporate towers, arrays, extraction silhouettes and crowns.
    landmarks = [
        ("buried corporate tower / Helix", 24, -54, 0.85, 0.25),
        ("research headquarters / Meridian", -4, -18, 0.68, 0.32),
        ("extraction facility / North Line", 18, 24, 0.48, 0.38),
        ("communications array / dead band", -27, 67, 0.74, 0.18),
        ("colonial maglev terminus", 10, 116, 0.34, 0.44),
        ("mass driver remnant", 36, 154, 0.58, 0.15),
    ]
    for name, lat, lon, height, width in landmarks:
        add_landmark(name, lat, lon, height, width, mats["ruin"], cols["02_COLONIAL_LANDMARKS"])

    for index, (lat, lon) in enumerate(((8, -80), (-12, -5), (23, 42))):
        point, normal = surface_point(lat, lon, RADIUS + 0.10)
        add_torus(
            f"Habitat Crown {index + 1:02d} / buried access ring",
            point,
            0.36,
            0.045,
            mats["crown"],
            cols["03_HABITAT_CROWNS"],
            normal,
        )
        add_curve(
            f"Crown {index + 1:02d} grounding trace",
            [point * 0.996, point * 1.04],
            mats["crown"],
            cols["03_HABITAT_CROWNS"],
            0.018,
        )

    # Forbidden borehole: visible as a dark, glowing wound, not an explorable site.
    point, normal = surface_point(-38, -124, RADIUS + 0.06)
    add_torus("FORBIDDEN CORE BOREHOLE / sealed", point, 0.56, 0.10, mats["borehole"], cols["04_CORE_BOREHOLE"], normal)
    bore = add_uv_sphere(
        "borehole depth marker",
        point + normal * 0.04,
        (0.38, 0.38, 0.05),
        mats["borehole"],
        cols["04_CORE_BOREHOLE"],
        32,
        16,
    )
    orient_to_normal(bore, normal)


def create_atmosphere(cols, mats):
    atmosphere = add_uv_sphere(
        "amber corrosive atmosphere / inspection shell",
        (0, 0, 0),
        (5.27, 5.27, 5.27),
        mats["atmosphere"],
        cols["01_STORM_SHELLS"],
        96,
        48,
    )
    mat = mats["atmosphere"]
    try:
        mat.surface_render_method = "DITHERED"
    except AttributeError:
        pass
    bsdf = next((node for node in mat.node_tree.nodes if node.bl_idname == "ShaderNodeBsdfPrincipled"), None)
    if bsdf and "Alpha" in bsdf.inputs:
        bsdf.inputs["Alpha"].default_value = 0.10
    return atmosphere


def setup_presentation(cols, mats):
    scene = bpy.context.scene
    scene.render.engine = (
        "BLENDER_EEVEE_NEXT"
        if "BLENDER_EEVEE_NEXT"
        in {item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
        else "BLENDER_EEVEE"
    )
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(OUT / "caligo_director_inspect.png")
    scene.render.film_transparent = False
    world = scene.world or bpy.data.worlds.new("Caligo Director World")
    scene.world = world
    world.color = (0.001, 0.002, 0.008)
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.001, 0.003, 0.012, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.12

    bpy.ops.object.camera_add(location=(0.0, -29.0, 1.8))
    camera = move_to(bpy.context.object, cols["05_PRESENTATION"])
    camera.name = "DIRECTOR CAMERA / 3-4 PLANET INSPECTION"
    camera.data.lens = 50
    camera.data.sensor_width = 36
    camera.rotation_euler = (Vector((0.0, 0.0, 0.0)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera

    bpy.ops.object.light_add(type="AREA", location=(-8.0, -10.0, 8.0))
    key = move_to(bpy.context.object, cols["05_PRESENTATION"])
    key.name = "warm sun / amber key"
    key.data.energy = 2600
    key.data.shape = "DISK"
    key.data.size = 7.0
    key.data.color = (1.0, 0.30, 0.08)
    key.rotation_euler = (Vector((0, 0, 0)) - key.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.light_add(type="AREA", location=(7.0, 3.0, 5.0))
    fill = move_to(bpy.context.object, cols["05_PRESENTATION"])
    fill.name = "magnetic aurora / cyan fill"
    fill.data.energy = 1800
    fill.data.size = 5.0
    fill.data.color = (0.05, 0.30, 1.0)
    fill.rotation_euler = (Vector((0, 0, 0)) - fill.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.light_add(type="AREA", location=(0.0, -12.0, -2.0))
    front = move_to(bpy.context.object, cols["05_PRESENTATION"])
    front.name = "soft camera fill / surface readability"
    front.data.energy = 950
    front.data.size = 8.0
    front.data.color = (1.0, 0.22, 0.06)
    front.rotation_euler = (Vector((0, 0, 0)) - front.location).to_track_quat("-Z", "Y").to_euler()

    # Large transparent-ish dust discs provide a readable storm silhouette.
    for i, z in enumerate((-3.5, -1.8, 0.2, 2.0, 3.7)):
        bpy.ops.mesh.primitive_torus_add(
            major_radius=7.1 + i * 0.12, minor_radius=0.015, major_segments=96, minor_segments=6, location=(0, 0, z)
        )
        ring = move_to(bpy.context.object, cols["01_STORM_SHELLS"])
        ring.name = f"dust shell latitude {i + 1}"
        ring.data.materials.append(mats["dust"])


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cols = {
        name: collection(name)
        for name in (
            "00_PLANET",
            "01_STORM_SHELLS",
            "02_COLONIAL_LANDMARKS",
            "03_HABITAT_CROWNS",
            "04_CORE_BOREHOLE",
            "05_PRESENTATION",
        )
    }
    mats = {
        "planet": planet_material(),
        "ruin": mat_principled("Colonial ruin / vitrified concrete", (0.12, 0.055, 0.028), 0.86, 0.2),
        "scar": emission_mat("Electrical scar / hot amber", (0.8, 0.12, 0.015), 4.0),
        "storm": emission_mat("Magnetic storm / electric blue", (0.02, 0.18, 0.9), 7.0),
        "crown": emission_mat("Habitat Crown / survivor signal", (0.02, 0.65, 0.85), 10.0),
        "borehole": emission_mat("Core borehole / forbidden red", (0.42, 0.008, 0.002), 3.5),
        "dust": mat_principled("Dust shell", (0.32, 0.055, 0.012), 1.0),
        "atmosphere": mat_principled(
            "Atmosphere / amber haze", (0.48, 0.035, 0.008), 1.0, 0.0, (0.18, 0.008, 0.001), 0.25
        ),
    }
    create_planet(cols, mats)
    create_landmarks(cols, mats)
    create_atmosphere(cols, mats)
    setup_presentation(cols, mats)

    scene = bpy.context.scene
    scene["caligo_asset_role"] = "Director planet inspection source"
    scene["caligo_notes"] = (
        "Open collections to inspect planet, storm shells, landmarks, Habitat Crowns and sealed core borehole."
    )
    OUT.mkdir(parents=True, exist_ok=True)
    blend_path = OUT / "caligo_director_inspect.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.render.render(write_still=True)
    print(f"Saved {blend_path}")


if __name__ == "__main__":
    main()

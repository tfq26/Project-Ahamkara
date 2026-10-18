#!/usr/bin/env python3
"""Build the compact Caligo Director destination node.

This scene is intentionally separate from the full planet inspection asset. It
is composed for a Destiny-style Director: a small atmospheric sphere, thin
navigation rings, a readable destination label, and two activity markers.

Run from the repository root with:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_caligo_director_node.py
"""

from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "models"


def link_collection(name: str):
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    return col


def move_to(obj, col):
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    col.objects.link(obj)
    return obj


def node_material(name, color, emission_strength=0.0, roughness=0.75, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission_strength:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def planet_material():
    mat = bpy.data.materials.new("Caligo node / orange beige storm desert")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 2.8
    noise.inputs["Detail"].default_value = 5.5
    noise.inputs["Roughness"].default_value = 0.72
    noise.inputs["Distortion"].default_value = 0.25
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.18
    ramp.color_ramp.elements[0].color = (0.035, 0.010, 0.004, 1.0)
    ramp.color_ramp.elements[1].position = 0.82
    ramp.color_ramp.elements[1].color = (0.76, 0.43, 0.18, 1.0)
    mid = ramp.color_ramp.elements.new(0.48)
    mid.color = (0.30, 0.075, 0.018, 1.0)
    high = ramp.color_ramp.elements.new(0.68)
    high.color = (0.58, 0.24, 0.065, 1.0)
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.18
    bump.inputs["Distance"].default_value = 0.08
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = 0.7
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def rim_material():
    mat = bpy.data.materials.new("Caligo node / pale amber atmospheric rim")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (1.0, 0.63, 0.25, 1.0)
    emission.inputs["Strength"].default_value = 2.2
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    layer = nodes.new("ShaderNodeLayerWeight")
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.38
    ramp.color_ramp.elements[0].color = (0.0, 0.0, 0.0, 1.0)
    ramp.color_ramp.elements[1].position = 0.68
    ramp.color_ramp.elements[1].color = (1.0, 1.0, 1.0, 1.0)
    mix = nodes.new("ShaderNodeMixShader")
    links.new(layer.outputs["Facing"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], mix.inputs["Fac"])
    links.new(transparent.outputs["BSDF"], mix.inputs[1])
    links.new(emission.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs[0], out.inputs["Surface"])
    return mat


def add_sphere(name, radius, mat, col, segments=128, rings=64):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=radius, location=(0, 0, 0))
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.data.materials.append(mat)
    bpy.ops.object.shade_smooth()
    return obj


def add_ring(name, major, minor, rotation, mat, col):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major,
        minor_radius=minor,
        major_segments=128,
        minor_segments=8,
        location=(0, 0, 0),
        rotation=rotation,
    )
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.data.materials.append(mat)
    return obj


def add_text(name, text, location, size, mat, col, align="LEFT"):
    bpy.ops.object.text_add(location=location, rotation=(math.pi / 2.0, 0.0, 0.0))
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.data.body = text
    obj.data.align_x = align
    obj.data.align_y = "CENTER"
    obj.data.size = size
    obj.data.extrude = 0.004
    obj.data.bevel_depth = 0.002
    obj.data.materials.append(mat)
    return obj


def add_marker(name, location, radius, mat, col):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=radius, location=location)
    obj = move_to(bpy.context.object, col)
    obj.name = name
    obj.data.materials.append(mat)
    bpy.ops.object.shade_smooth()
    return obj


def add_background(scene, col):
    mat = node_material("Director deep teal background", (0.002, 0.018, 0.040), emission_strength=0.04)
    bpy.ops.mesh.primitive_plane_add(size=40.0, location=(0.0, 2.0, 0.0), rotation=(math.pi / 2.0, 0.0, 0.0))
    plane = move_to(bpy.context.object, col)
    plane.name = "Director backdrop"
    plane.data.materials.append(mat)


def export_engine_globe(source_obj):
    """Export only the globe with material-region color baked into its faces."""
    export_obj = source_obj.copy()
    export_obj.data = source_obj.data.copy()
    export_obj.name = "caligo_director_node_engine"
    bpy.context.scene.collection.objects.link(export_obj)
    export_obj.location = (0.0, 0.0, 0.0)

    palette = (
        ("Caligo engine / deep storm", (0.055, 0.018, 0.008)),
        ("Caligo engine / burnt rust", (0.28, 0.055, 0.010)),
        ("Caligo engine / desert orange", (0.58, 0.18, 0.025)),
        ("Caligo engine / ochre dune", (0.72, 0.34, 0.090)),
        ("Caligo engine / sand beige", (0.72, 0.54, 0.30)),
    )
    export_obj.data.materials.clear()
    for name, color in palette:
        export_obj.data.materials.append(node_material(name, color, roughness=0.86))

    # Stable low-frequency regions read as weather systems and desert basins
    # at Director scale, while exporting as ordinary glTF material primitives.
    for polygon in export_obj.data.polygons:
        center = polygon.center.normalized()
        value = (
            math.sin(center.x * 5.7 + center.z * 2.1)
            + 0.65 * math.sin(center.y * 9.2 - center.x * 3.4)
            + 0.35 * math.cos(center.z * 13.0 + center.y * 4.0)
        )
        if center.x < -0.32 and center.z < 0.25:
            value -= 1.0
        thresholds = (-1.10, -0.35, 0.35, 1.05)
        polygon.material_index = sum(value > threshold for threshold in thresholds)

    bpy.ops.object.select_all(action="DESELECT")
    export_obj.select_set(True)
    bpy.context.view_layer.objects.active = export_obj
    bpy.ops.export_scene.gltf(
        filepath=str(OUT / "caligo_director_node.gltf"),
        export_format="GLTF_SEPARATE",
        export_yup=True,
        use_selection=True,
        export_animations=False,
        export_materials="EXPORT",
    )
    bpy.data.objects.remove(export_obj, do_unlink=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    collections = {
        "PLANET NODE": link_collection("PLANET NODE"),
        "ORBIT RINGS": link_collection("ORBIT RINGS"),
        "ACTIVITY MARKERS": link_collection("ACTIVITY MARKERS"),
        "LABEL": link_collection("LABEL"),
        "PRESENTATION": link_collection("PRESENTATION"),
    }

    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in {item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items} else "BLENDER_EEVEE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 600
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(OUT / "caligo_director_node.png")
    scene.render.film_transparent = False
    world = scene.world or bpy.data.worlds.new("Caligo Node World")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.001, 0.010, 0.024, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.08

    planet = planet_material()
    ring = node_material("Navigation ring / muted amber", (0.65, 0.27, 0.07), emission_strength=2.6)
    label = node_material("Destination label / soft white", (0.72, 0.84, 0.96), emission_strength=0.5)
    yellow = node_material("Activity marker / available", (0.90, 0.68, 0.04), emission_strength=5.0)
    white = node_material("Activity marker / discovered", (0.72, 0.82, 0.92), emission_strength=3.0)

    globe = add_sphere("CALIGO / atmospheric destination globe", 1.68, planet, collections["PLANET NODE"])

    # Keep the destination node to the right of the information block, as in
    # the Director reference composition.
    for obj in list(collections["PLANET NODE"].objects):
        obj.location.x += 1.05

    add_text("destination name", "CALIGO", (-3.15, -0.03, 1.30), 0.40, label, collections["LABEL"])
    add_text("destination subtitle", "THE BURIED WORLD", (-3.12, -0.03, 0.98), 0.15, ring, collections["LABEL"])
    add_marker("surface ruins activity", (-2.58, -0.04, 0.61), 0.105, yellow, collections["ACTIVITY MARKERS"])
    add_marker("crown activity", (-2.24, -0.04, 0.61), 0.095, white, collections["ACTIVITY MARKERS"])
    add_text("activity caption", "SURFACE RUINS", (-3.12, -0.03, 0.29), 0.12, label, collections["LABEL"])

    add_background(scene, collections["PRESENTATION"])

    bpy.ops.object.camera_add(location=(0.0, -13.5, 0.0))
    camera = move_to(bpy.context.object, collections["PRESENTATION"])
    camera.name = "DIRECTOR NODE CAMERA"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 7.2
    camera.rotation_euler = (math.pi / 2.0, 0.0, 0.0)
    scene.camera = camera

    bpy.ops.object.light_add(type="AREA", location=(-4.0, -6.0, 5.0))
    key = move_to(bpy.context.object, collections["PRESENTATION"])
    key.name = "warm desert key"
    key.data.energy = 900
    key.data.size = 5.0
    key.data.color = (1.0, 0.50, 0.16)
    key.rotation_euler = (Vector((0, 0, 0)) - key.location).to_track_quat("-Z", "Y").to_euler()

    bpy.ops.object.light_add(type="AREA", location=(4.0, -3.0, -3.0))
    fill = move_to(bpy.context.object, collections["PRESENTATION"])
    fill.name = "cool magnetic storm fill"
    fill.data.energy = 260
    fill.data.size = 4.0
    fill.data.color = (0.10, 0.38, 0.70)
    fill.rotation_euler = (Vector((0, 0, 0)) - fill.location).to_track_quat("-Z", "Y").to_euler()

    scene["caligo_asset_role"] = "Compact Director destination node"
    scene["caligo_reference_direction"] = "Compact orange-beige storm world with a clean planetary edge, label and activity markers"
    OUT.mkdir(parents=True, exist_ok=True)
    blend_path = OUT / "caligo_director_node.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.render.render(write_still=True)
    export_engine_globe(globe)
    print(f"Saved {blend_path}")


if __name__ == "__main__":
    main()

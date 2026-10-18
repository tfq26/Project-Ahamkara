#!/usr/bin/env python3
"""Export the evaluated neutral human mesh used by the Character View.

The animated MPFB source remains authoritative for gameplay motion.  This
separate export applies its armature's neutral rest deformation into vertices,
giving the menu a reliable full-body profile mesh while the engine's 53-joint
skinning work continues independently.

Run from the repository root:

  Blender -b assets/models/operative_human_base.blend \
    -P tools/blender/export_operative_profile_static.py -- --out-dir assets/models
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy

SOURCE_MESH = "Ahamkara_Operative_Human_Base"
ASSET_NAME = "operative_human_profile_static"


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path("assets/models"))
    return parser.parse_args(argv)


def main() -> None:
    args = parse_args()
    source = bpy.data.objects.get(SOURCE_MESH)
    if source is None or source.type != "MESH":
        raise RuntimeError(f"Expected mesh {SOURCE_MESH}")

    # Evaluating through the dependency graph applies the armature modifier in
    # Blender, exactly as the authoring preview does. The resulting object has
    # no skin, bones, or animation channels by design.
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = source.evaluated_get(depsgraph)
    profile_mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=depsgraph)
    profile_mesh.materials.clear()
    for material in source.data.materials:
        profile_mesh.materials.append(material)

    profile = bpy.data.objects.new(ASSET_NAME, profile_mesh)
    bpy.context.collection.objects.link(profile)
    profile.matrix_world = source.matrix_world.copy()

    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    profile.select_set(True)
    bpy.context.view_layer.objects.active = profile

    args.out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=str((args.out_dir / f"{ASSET_NAME}.gltf").resolve()),
        export_format="GLTF_SEPARATE",
        use_selection=True,
        export_animations=False,
        export_skins=False,
        export_materials="EXPORT",
        export_yup=True,
    )
    print(f"Generated {args.out_dir / (ASSET_NAME + '.gltf')}")


if __name__ == "__main__":
    main()

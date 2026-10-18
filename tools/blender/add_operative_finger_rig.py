#!/usr/bin/env python3
"""Validate production-style finger chains and weights on the operative source rig.

The MPFB import already has a complete hand mesh, named finger joints, and
finger weights. This tool validates that contract before asset export; it does
not rewrite authored finger bones or body weights.

Run from the repository root::

  Blender -b assets/models/operative_human_base.blend \
    -P tools/blender/add_operative_finger_rig.py -- --out assets/models/operative_human_base.blend
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy

RIG_NAME = "Ahamkara_Operative_GameRig"
MESH_NAME = "Ahamkara_Operative_Human_Base"
FINGERS = ("index", "middle", "ring", "pinky")


def parse_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    return parser.parse_args(argv)


def validate_finger_bones(rig: bpy.types.Object) -> None:
    """Reject an incomplete source rig without rewriting its authored bones."""
    names = {bone.name for bone in rig.data.bones}
    expected = {f"{finger}_{joint:02d}_{side}" for side in ("l", "r") for finger in FINGERS for joint in (1, 2, 3)}
    missing = sorted(expected - names)
    if missing:
        raise RuntimeError("Operative rig is missing finger joints: " + ", ".join(missing))


def validate_finger_weights(mesh: bpy.types.Object) -> None:
    """Ensure each authored finger chain owns visible hand vertices."""
    for side in ("l", "r"):
        chain_groups = [
            mesh.vertex_groups.get(f"{finger}_{joint:02d}_{side}") for finger in FINGERS for joint in (1, 2, 3)
        ]
        if any(group is None for group in chain_groups):
            raise RuntimeError(f"{side}-hand mesh is missing one or more finger vertex groups")
        owned = sum(
            1
            for vertex in mesh.data.vertices
            if any(
                entry.group in {group.index for group in chain_groups} and entry.weight > 0.05
                for entry in vertex.groups
            )
        )
        if owned < 160:
            raise RuntimeError(f"Only {owned} {side}-hand vertices are weighted to finger chains")
        print(f"Validated {owned} {side}-hand finger-weighted vertices")


def main() -> None:
    args = parse_args()
    rig = bpy.data.objects.get(RIG_NAME)
    mesh = bpy.data.objects.get(MESH_NAME)
    if rig is None or rig.type != "ARMATURE" or mesh is None or mesh.type != "MESH":
        raise RuntimeError("Expected operative armature and human mesh in the opened blend file")
    validate_finger_bones(rig)
    validate_finger_weights(mesh)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(args.out.resolve()))
    print(f"Saved full-hand operative rig to {args.out}")


if __name__ == "__main__":
    main()

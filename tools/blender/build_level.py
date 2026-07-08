#!/usr/bin/env python3
"""Headless Blender generator for Ahamkara levels ("Path B").

Reads the SAME canonical JSON level spec used by Path A
(tools/levelgen/spec_to_lvl.py) and produces, in one pass:

  - blockout geometry as glTF (collision volumes become greybox boxes; mesh
    instances become placement empties named by their asset id),
  - a saved `.blend` a human can open and refine,
  - the `.lvl` text file (written by the *shared* Path A writer, so both paths
    emit byte-identical `.lvl` for the same spec).

Generation is one-way: the spec owns layout/semantics; Blender owns geometry
detail. Hand edits to the `.blend` do not round-trip back into the spec.

Headless usage (requires a Blender install; tested against Blender 4.x):

    blender -b -P tools/blender/build_level.py -- <spec.json> <out_dir>

This writes <out_dir>/<name>.blend, <out_dir>/<name>.gltf, and
<out_dir>/<name>.lvl.

The pure (bpy-free) logic — spec loading, the blockout plan, and `.lvl` writing —
lives in module-level functions so it is unit-testable without Blender
(see tools/blender/test_build_level.py).
"""

import json
import sys
from pathlib import Path

# Reuse the Path A `.lvl` writer so both authoring paths share one emitter.
_LEVELGEN_DIR = Path(__file__).resolve().parent.parent / "levelgen"
if str(_LEVELGEN_DIR) not in sys.path:
    sys.path.insert(0, str(_LEVELGEN_DIR))
import spec_to_lvl  # noqa: E402  (path adjusted above)

# bpy is only available when run inside Blender. Guard it so this module can be
# imported (and its pure logic tested) in a plain Python interpreter.
try:
    import bpy  # type: ignore
    HAVE_BPY = True
except ImportError:  # pragma: no cover - exercised only outside Blender
    bpy = None
    HAVE_BPY = False


# ---------------------------------------------------------------------------
# Pure, bpy-free logic (unit-tested)
# ---------------------------------------------------------------------------

def load_spec(path: str) -> dict:
    return json.loads(Path(path).read_text())


def plan_blockout(spec: dict) -> dict:
    """Translate a level spec into a bpy-agnostic build plan.

    Each collision AABB becomes a box (center + size); each mesh instance
    becomes a placement; spawns are passed through. Pure: no Blender calls.
    """
    boxes = []
    for b in spec.get("collision", []):
        mn = b["min"]
        mx = b["max"]
        top = float(b.get("top_y", 0.0))
        bot = float(b.get("bottom_y", 0.0))
        boxes.append({
            "center": [(mn[0] + mx[0]) / 2.0, (top + bot) / 2.0, (mn[1] + mx[1]) / 2.0],
            "size": [abs(mx[0] - mn[0]), abs(top - bot), abs(mx[1] - mn[1])],
            "wall": bool(b.get("wall", False)),
        })

    instances = []
    for m in spec.get("meshes", []):
        instances.append({
            "mesh": m["mesh"],
            "pos": list(m.get("pos", [0.0, 0.0, 0.0])),
            "yaw": float(m.get("yaw", 0.0)),
            "scale": list(m.get("scale", [1.0, 1.0, 1.0])),
        })

    spawns = [{"pos": list(s["pos"]), "yaw": float(s.get("yaw", 0.0))}
              for s in spec.get("spawns", [])]

    return {"boxes": boxes, "instances": instances, "spawns": spawns}


def write_lvl(spec: dict, out_path: Path, source_name: str = "") -> None:
    """Write the `.lvl` using the shared Path A writer."""
    out_path.write_text(spec_to_lvl.spec_to_lvl_text(spec, source_name))


def output_basename(spec: dict) -> str:
    name = str(spec.get("name", "level")).strip().lower()
    return "".join(c if c.isalnum() else "_" for c in name).strip("_") or "level"


# ---------------------------------------------------------------------------
# Blender-dependent build (only runs inside Blender)
# ---------------------------------------------------------------------------

def build_in_blender(plan: dict, out_dir: Path, basename: str) -> None:  # pragma: no cover
    if not HAVE_BPY:
        raise RuntimeError("build_in_blender requires Blender's bpy module")

    # Fresh scene.
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # Greybox boxes from collision volumes.
    for i, box in enumerate(plan["boxes"]):
        cx, cy, cz = box["center"]
        sx, sy, sz = box["size"]
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(cx, cz, cy))
        obj = bpy.context.active_object
        obj.name = f"collision_{i}"
        obj.scale = (max(sx, 0.01), max(sz, 0.01), max(sy, 0.01))

    # Placement empties for mesh instances (the .lvl carries the real ref).
    for i, inst in enumerate(plan["instances"]):
        px, py, pz = inst["pos"]
        bpy.ops.object.empty_add(type="ARROWS", location=(px, pz, py))
        obj = bpy.context.active_object
        obj.name = f"mesh_{i}_{Path(inst['mesh']).stem}"

    out_dir.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(out_dir / f"{basename}.blend"))
    bpy.ops.export_scene.gltf(filepath=str(out_dir / f"{basename}.gltf"),
                              export_format="GLTF_SEPARATE")


def main(argv: list[str]) -> int:
    # When launched as `blender -b -P build_level.py -- spec out_dir`, the script
    # args follow "--". When run as a plain script, take argv[1:].
    if "--" in argv:
        args = argv[argv.index("--") + 1:]
    else:
        args = argv[1:]

    if len(args) != 2:
        print("usage: build_level.py <spec.json> <out_dir>", file=sys.stderr)
        print("  (run via: blender -b -P tools/blender/build_level.py -- spec.json out_dir)",
              file=sys.stderr)
        return 2

    spec_path, out_dir_str = args[0], args[1]
    spec = load_spec(spec_path)
    plan = plan_blockout(spec)
    basename = output_basename(spec)
    out_dir = Path(out_dir_str)
    out_dir.mkdir(parents=True, exist_ok=True)

    write_lvl(spec, out_dir / f"{basename}.lvl", Path(spec_path).name)

    if HAVE_BPY:
        build_in_blender(plan, out_dir, basename)
        print(f"build_level: wrote {basename}.blend, {basename}.gltf, {basename}.lvl to {out_dir}")
    else:
        print("build_level: bpy not available; wrote .lvl only. "
              "Run inside Blender to also produce .blend and glTF.", file=sys.stderr)
        print(f"build_level: wrote {basename}.lvl to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

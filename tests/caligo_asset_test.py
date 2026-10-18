#!/usr/bin/env python3
"""Pure validation for the Caligo source contracts."""

import importlib.util
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_generator():
    path = ROOT / "tools" / "blender" / "build_caligo.py"
    spec = importlib.util.spec_from_file_location("build_caligo", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def main() -> int:
    generator = load_generator()
    assert generator.object_count_for_lod(0)[0] > generator.object_count_for_lod(1)[0]
    assert generator.object_count_for_lod(1)[0] > generator.object_count_for_lod(2)[0]
    assert generator.blender_location((4.0, 5.0, 6.0)) == (4.0, -6.0, 5.0)
    assert generator.blender_size((4.0, 5.0, 6.0)) == (4.0, 6.0, 5.0)

    director = json.loads((ROOT / "assets/director/caligo_director.json").read_text())
    assert director["id"] == "caligo"
    assert len(director["activities"]) == 1
    assert director["activities"][0]["level"].endswith("caligo_surface_ruins.aelevel")
    assert director["layers"] == ["SURFACE", "HABITAT", "UNDERWORKS", "DEEP CALIGO"]

    level = json.loads((ROOT / "assets/levels/caligo_surface_ruins.json").read_text())
    assert level["name"] == "Caligo Surface Ruins"
    assert level["spawns"] == [{"pos": [0, 2.0, -12], "yaw": 0, "team": 0}]
    assert len(level["collision"]) >= 1
    assert len(level["collision_meshes"]) == 1
    assert len(level["meshes"]) == 1
    launcher = ROOT / "tools/blender/run_caligo.sh"
    assert launcher.is_file()
    assert os.access(launcher, os.X_OK)

    flashback_main = (ROOT / "samples/flashback/src/main.cpp").read_text()
    assert "kDefaultShowcaseLevel = nullptr" in flashback_main
    assert "prototype_box.aelevel" not in flashback_main

    director_menu = json.loads((ROOT / "assets/menus/director.json").read_text())
    deploy_actions = [
        element.get("action")
        for panel in director_menu["elements"]
        for element in panel.get("elements", [])
        if element.get("label") == "DEPLOY"
    ]
    assert deploy_actions == ["start_game:assets/compiled/levels/caligo_surface_ruins.aelevel"]
    print("caligo_asset_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

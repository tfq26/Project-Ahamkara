#!/usr/bin/env python3
"""Validate the reproducible external-M4A1 import contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    source = ROOT / "assets/source_models/m4a1_cc0"
    assert (source / "M4A1.fbx").is_file()
    for texture in ("M4A1_Base_Color.png", "M4A1_Metallic.png", "M4A1_Normal.png", "M4A1_Roughness.png"):
        assert (source / texture).is_file(), texture

    importer = (ROOT / "tools/blender/import_cc0_m4a1_carbine.py").read_text()
    assert "SOURCE_RIGHT_HAND_GRIP" in importer
    assert "SOURCE_LEFT_HAND_GRIP" in importer
    assert "SOURCE_STOCK_SHOULDER" in importer
    assert "SOURCE_MUZZLE" in importer
    assert "math.radians(-90.0)" in importer
    assert 'add_socket(carbine, "stock_shoulder"' in importer
    assert 'add_socket(carbine, "muzzle"' in importer
    assert (ROOT / "tools/blender/render_operative_rifle_profile_preview.py").is_file()

    attribution = (ROOT / "docs/third-party/m4a1-cc0.md").read_text()
    assert "CC0" in attribution
    assert "opengameart.org/content/m4a1-assault-rifle" in attribution
    print("external_m4a1_asset_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

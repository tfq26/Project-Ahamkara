#!/usr/bin/env python3
"""Validate the authored character and dance vertical-slice contracts."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    gltf_path = ROOT / "assets/models/caligo_field_suit.gltf"
    model = json.loads(gltf_path.read_text())

    assert len(model.get("meshes", [])) >= 20, "field suit should be layered, not a debug box"
    assert len(model.get("skins", [])) == 1
    assert 20 <= len(model["skins"][0]["joints"]) <= 64

    animations = {animation.get("name"): animation for animation in model.get("animations", [])}
    assert {"profile_stance", "dance_charleston", "locomotion_walk", "locomotion_run"} <= animations.keys()
    stance = animations["profile_stance"]
    assert len(stance.get("channels", [])) >= 60, "profile stance must pose the complete rig"
    dance = animations["dance_charleston"]
    assert len(dance.get("channels", [])) >= 60, "dance must animate the full body"
    assert len(animations["locomotion_walk"].get("channels", [])) >= 60
    assert len(animations["locomotion_run"].get("channels", [])) >= 60

    primitives = [
        primitive
        for mesh in model["meshes"]
        for primitive in mesh.get("primitives", [])
    ]
    assert primitives
    assert all("JOINTS_0" in primitive.get("attributes", {}) for primitive in primitives)
    assert all("WEIGHTS_0" in primitive.get("attributes", {}) for primitive in primitives)

    assert (ROOT / "assets/models/caligo_field_suit.blend").is_file()
    assert (ROOT / "assets/compiled/models/caligo_field_suit.aemesh").is_file()
    attribution = ROOT / "docs/third-party/cmu-charleston-93-02.md"
    assert attribution.is_file()
    assert "mocap.cs.cmu.edu/subjects/93" in attribution.read_text()
    locomotion_attribution = ROOT / "docs/third-party/cmu-locomotion-35.md"
    assert locomotion_attribution.is_file()
    assert "subjectnumber=35" in locomotion_attribution.read_text()

    manifest = (ROOT / "assets/manifest.assets").read_text()
    assert "models/caligo_field_suit.gltf compiled/models/caligo_field_suit.aemesh" in manifest

    # Presentation avatars must read the persisted profile directly rather
    # than keeping another suit-color copy in the renderer.
    character_header = (ROOT / "client/include/ahamkara/client/animated_character.h").read_text()
    character_source = (ROOT / "client/src/animated_character.cpp").read_text()
    pipeline = (ROOT / "client/src/client_frame_pipeline.cpp").read_text()
    assert "set_appearance_source(const CharacterCustomization& appearance)" in character_header
    assert "const CharacterCustomization* appearance_" in character_header
    assert "appearance.suit_color_r" in character_source
    assert "animated_character_.set_appearance_source(client_config_.character)" in pipeline
    assert "assets/compiled/models/operative_human_base.aemesh" in pipeline
    assert "assets/compiled/models/operative_human_rifle_idle_static.aemesh" in pipeline
    assert "0.0F, 1.0F, 0.0F, 0.0F" in pipeline
    assert "0.0F, 0.0F, 1.0F, 0.0F" in pipeline
    assert "third_person_character_visible" in pipeline
    assert "character_dance_active_" in pipeline
    assert "KeyCode::F6" in pipeline
    assert "AHAMKARA_GAME_MCP_START_EMOTE" in pipeline
    assert "character_presentation_background" in pipeline
    assert "render_character_view_foreground" in pipeline
    assert "animated_character_.render_static" in pipeline
    assert "GL_SCISSOR_TEST" in pipeline
    assert "camera_position = {0.0F, 0.80F, -4.5F}" in pipeline
    assert 'animation_.play("profile_stance", true)' in character_source
    assert 'pose_ = animation_.rest_pose();' in character_source
    assert "uUseSkinning" in character_source
    assert "void AnimatedCharacter::render_static" in character_source
    assert "(!use_skinning && mesh_index == 0) || is_primary_suit_material(mesh)" in character_source
    assert "backend_->draw_gpu_mesh_skinned(mesh);" in character_source
    backend_source = (ROOT / "engine/render/src/render_backend_opengl.cpp").read_text()
    assert "if (vbo_joints != 0)" in backend_source
    assert "glVertexAttrib4f(1, 0.0F, 0.0F, 0.0F, 0.0F)" in backend_source
    assert "if (vbo_weights != 0)" in backend_source
    assert (ROOT / "tools/blender/add_character_profile_stance.py").is_file()
    print("character_asset_test: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

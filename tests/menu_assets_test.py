"""Validate the data-driven Flashback menu contract without launching a window."""

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MENU_DIR = ROOT / "assets" / "menus"


def _load(name: str) -> dict:
    with (MENU_DIR / name).open(encoding="utf-8") as stream:
        return json.load(stream)


def _walk(elements: list[dict]):
    for element in elements:
        yield element
        yield from _walk(element.get("elements", []))


def run_contract_checks() -> None:
    test_case = FlashbackMenuAssetsTest()
    test_case.test_flashback_menu_screens_are_valid_json()
    test_case.test_main_menu_has_real_game_entry_points()
    test_case.test_main_menu_uses_cinematic_background_and_mixed_coordinates()
    test_case.test_character_view_is_a_dedicated_showcase_stage()
    test_case.test_menu_theme_exposes_the_flashback_visual_system()
    test_case.test_gameplay_hud_uses_a_single_combat_cluster()


class FlashbackMenuAssetsTest(unittest.TestCase):
    def test_flashback_menu_screens_are_valid_json(self):
        self.assertFalse((MENU_DIR / "map_select.json").exists())
        for path in MENU_DIR.glob("*.json"):
            with path.open(encoding="utf-8") as stream:
                json.load(stream)


    def test_main_menu_has_real_game_entry_points(self):
        elements = list(_walk(_load("main_menu.json")["elements"]))
        actions = {element.get("action") for element in elements}
        labels = {element.get("label") for element in elements}

        self.assertIn("push_screen:director", actions)
        self.assertIn("push_screen:character_view", actions)
        self.assertIn("start_sandbox", actions)
        self.assertIn("quit_application", actions)
        self.assertTrue({"DEPLOY", "CHARACTER", "SANDBOX", "QUIT TO DESKTOP"} <= labels)


    def test_main_menu_uses_cinematic_background_and_mixed_coordinates(self):
        menu = _load("main_menu.json")
        panel = menu["elements"][0]
        positioned_children = [
            element for element in panel["elements"] if "x" in element and "y" in element
        ]

        layers = menu["background_layers"]
        self.assertEqual([layer["image"] for layer in layers], [
            "assets/compiled/textures/flashback_menu_sky.aetex",
            "assets/compiled/textures/flashback_menu_kingdom.aetex",
            "assets/compiled/textures/flashback_menu_foreground.aetex",
        ])
        self.assertEqual(len(layers), 3)
        self.assertLess(layers[0]["drift"], layers[1]["drift"])
        self.assertLess(layers[1]["drift"], layers[2]["drift"])
        self.assertTrue(all(layer["zoom"] > 1.0 for layer in layers))
        self.assertGreater(layers[1]["offset_y"], 0.0)
        self.assertGreaterEqual(layers[0]["drift"], 0.012)
        self.assertTrue(menu["cinematic_background"])
        self.assertGreater(menu["background_drift"], 0.0)
        self.assertGreater(menu["background_zoom"], 1.0)
        self.assertLess(menu["background"][3], 0.5)
        self.assertEqual(panel["color"], "none")
        self.assertTrue(any(0.0 <= element["x"] <= 1.0 for element in positioned_children))
        self.assertTrue(any(element["y"] > 1.0 for element in positioned_children))

        buttons = [element for element in positioned_children if element["type"] == "button"]
        self.assertTrue(buttons)
        self.assertTrue(all(element["style"] == "menu" for element in buttons))


    def test_character_view_is_a_dedicated_showcase_stage(self):
        self.assertFalse((MENU_DIR / "settings.json").exists())

        view = _load("character_view.json")
        elements = list(_walk(view["elements"]))
        self.assertEqual(elements, [])
        self.assertEqual(view["background"], [0.0, 0.0, 0.0, 0.0])

        main_actions = {
            element.get("action") for element in _walk(_load("main_menu.json")["elements"])
        }
        pause_actions = {
            element.get("action") for element in _walk(_load("pause_menu.json")["elements"])
        }
        self.assertIn("push_screen:character_view", main_actions)
        self.assertIn("push_screen:character_view", pause_actions)
        self.assertNotIn("push_screen:settings", main_actions | pause_actions)
        self.assertNotIn("push_screen:map_select", main_actions | pause_actions)


    def test_menu_theme_exposes_the_flashback_visual_system(self):
        theme = _load("theme.json")
        colors = theme["colors"]
        fonts = theme["fonts"]

        for name in ("accent", "accent_cool", "panel", "panel_elevated", "text_muted"):
            self.assertIn(name, colors)
        self.assertGreater(fonts["title"], fonts["header"])
        self.assertGreater(fonts["header"], fonts["body"])


    def test_gameplay_hud_uses_a_single_combat_cluster(self):
        hud = _load("hud.json")
        elements = hud["elements"]
        element_types = [element["type"] for element in elements]

        self.assertEqual(element_types, ["field_frame", "crosshair"])
        field_frame = elements[0]
        self.assertEqual(field_frame["anchor"], "bottom_left")
        self.assertEqual(field_frame["width"], 0)
        self.assertLessEqual(field_frame["y"], 0.96)


if __name__ == "__main__":
    unittest.main()

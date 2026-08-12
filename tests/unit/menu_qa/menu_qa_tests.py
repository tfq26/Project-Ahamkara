#!/usr/bin/env python3
"""Unit tests for the Flashback menu visual QA static checker.

Covers the acceptance-criteria checks: structure, typography orientation,
contrast, focus visibility, alignment, and clipping.
"""

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

CHECKER_PATH = Path(__file__).resolve().parents[3] / "tools" / "menu_qa" / "menu_qa_check.py"
SPEC = importlib.util.spec_from_file_location("menu_qa_check", CHECKER_PATH)
assert SPEC is not None and SPEC.loader is not None
checker = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = checker
SPEC.loader.exec_module(checker)


def make_menus_dir(theme: dict, screens: dict[str, dict]) -> Path:
    path = Path(tempfile.mkdtemp())
    path.joinpath("theme.json").write_text(json.dumps(theme), encoding="utf-8")
    for name, screen in screens.items():
        path.joinpath(f"{name}.json").write_text(json.dumps(screen), encoding="utf-8")
    return path


def cleanup_menus_dir(path: Path) -> None:
    shutil.rmtree(path, ignore_errors=True)


THEME = {
    "colors": {
        "bg": [0.06, 0.08, 0.12, 1.0],
        "panel": [0.08, 0.10, 0.16, 1.0],
        "accent": [0.20, 0.55, 0.90, 1.0],
        "text_primary": [0.94, 0.95, 0.97, 1.0],
        "text_secondary": [0.55, 0.58, 0.65, 1.0],
        "button_primary": [0.20, 0.55, 0.90, 0.85],
        "button_primary_hover": [0.30, 0.65, 1.00, 0.95],
        "button_secondary": [0.18, 0.20, 0.28, 0.70],
        "button_secondary_hover": [0.24, 0.27, 0.38, 0.85],
    },
    "fonts": {"title": 2.8, "header": 1.6, "body": 1.0, "small": 0.85, "button": 1.05},
    "spacing": {"button_width": 280, "button_height": 48},
}


def good_screen(name: str = "main_menu") -> dict:
    return {
        "name": name,
        "background": [0.04, 0.06, 0.10, 1.0],
        "elements": [
            {
                "type": "panel",
                "x": 0.5,
                "y": 0.5,
                "anchor": "center",
                "width": 480,
                "height": 520,
                "color": "panel",
                "elements": [
                    {
                        "type": "text",
                        "content": "TITLE",
                        "font": "title",
                        "color": "text_primary",
                        "x": 0.5,
                        "y": 0.1,
                        "anchor": "top_center",
                    },
                    {
                        "type": "button",
                        "label": "PLAY",
                        "style": "primary",
                        "action": "start_game",
                        "x": 0.5,
                        "y": 0.3,
                        "anchor": "top_center",
                    },
                ],
            }
        ],
    }


class ContrastTests(unittest.TestCase):
    def test_contrast_ratio_known_value(self) -> None:
        # White text on near-black background.
        white = [1.0, 1.0, 1.0, 1.0]
        black = [0.0, 0.0, 0.0, 1.0]
        self.assertAlmostEqual(checker.contrast_ratio(white, black), 21.0, delta=0.1)

    def test_contrast_ratio_symmetric(self) -> None:
        fg = [0.2, 0.55, 0.9, 1.0]
        bg = [0.06, 0.08, 0.12, 1.0]
        self.assertAlmostEqual(checker.contrast_ratio(fg, bg), checker.contrast_ratio(bg, fg), delta=1e-9)

    def test_luminance_black_and_white(self) -> None:
        self.assertAlmostEqual(checker.relative_luminance([0, 0, 0, 1]), 0.0)
        self.assertAlmostEqual(checker.relative_luminance([1, 1, 1, 1]), 1.0)


class ComputePositionTests(unittest.TestCase):
    def test_center(self) -> None:
        self.assertEqual(checker.compute_position(640, 360, "center", 0, 0, 1280, 720, 480, 520), (400.0, 100.0))

    def test_top_center(self) -> None:
        self.assertEqual(checker.compute_position(240, 100, "top_center", 400, 100, 480, 520, 64, 32), (608.0, 200.0))

    def test_bottom_center(self) -> None:
        self.assertEqual(
            checker.compute_position(640, 360, "bottom_center", 0, 0, 1280, 720, 480, 520), (400.0, -160.0)
        )

    def test_top_left_and_top_right(self) -> None:
        self.assertEqual(checker.compute_position(20, 20, "top_left", 0, 0, 1280, 720, 100, 100), (20.0, 20.0))
        self.assertEqual(checker.compute_position(20, 20, "top_right", 0, 0, 1280, 720, 100, 100), (-80.0, 20.0))

    def test_unknown_anchor_falls_back_to_base(self) -> None:
        self.assertEqual(checker.compute_position(50, 60, "", 10, 20, 100, 100, 30, 30), (60.0, 80.0))


class RectTests(unittest.TestCase):
    def test_contains_inside(self) -> None:
        parent = checker.Rect(0, 0, 100, 100)
        self.assertTrue(parent.contains(checker.Rect(10, 10, 20, 20)))

    def test_contains_outside(self) -> None:
        parent = checker.Rect(0, 0, 100, 100)
        self.assertFalse(parent.contains(checker.Rect(90, 90, 20, 20)))


class StructureTests(unittest.TestCase):
    def test_missing_theme_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            checker_inst = checker.MenuQaChecker(Path(temp_dir), {}, [[1280, 720]])
            findings = checker_inst.run()
        self.assertTrue(any(f.check == "structure" and f.severity == "error" for f in findings))

    def test_missing_scoped_screen_is_error(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["settings"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "structure" and "settings.json" in f.message for f in findings))

    def test_invalid_json_is_error(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir)
            path.joinpath("theme.json").write_text("{not json", encoding="utf-8")
            checker_inst = checker.MenuQaChecker(path, {}, [[1280, 720]])
            findings = checker_inst.run()
        self.assertTrue(any(f.check == "structure" and f.severity == "error" for f in findings))


class TypographyTests(unittest.TestCase):
    def test_unknown_font_is_error(self) -> None:
        screen = good_screen()
        screen["elements"][0]["elements"][0]["font"] = "does_not_exist"
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "typography" and "unknown font" in f.message for f in findings))

    def test_empty_text_is_warning(self) -> None:
        screen = good_screen()
        screen["elements"][0]["elements"][0]["content"] = "   "
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(
            any(f.check == "typography" and f.severity == "warning" and "empty content" in f.message for f in findings)
        )

    def test_valid_text_has_no_typography_error(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertFalse(any(f.check == "typography" and f.severity == "error" for f in findings))


class ContrastCheckTests(unittest.TestCase):
    def test_low_contrast_text_is_error(self) -> None:
        screen = good_screen()
        # A grey close to the panel background has a contrast ratio below 4.5:1.
        theme = json.loads(json.dumps(THEME))
        theme["colors"]["low_contrast"] = [0.12, 0.14, 0.20, 1.0]
        screen["elements"][0]["elements"][0]["color"] = "low_contrast"
        menus = make_menus_dir(theme, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "contrast" and f.severity == "error" for f in findings))

    def test_high_contrast_text_passes(self) -> None:
        screen = good_screen()
        screen["elements"][0]["elements"][0]["color"] = "text_primary"
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertFalse(any(f.check == "contrast" and f.severity == "error" for f in findings))


class FocusVisibilityTests(unittest.TestCase):
    def test_missing_hover_colour_is_error(self) -> None:
        theme = json.loads(json.dumps(THEME))
        del theme["colors"]["button_primary_hover"]
        menus = make_menus_dir(theme, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "focus_visibility" and "hover" in f.message for f in findings))

    def test_missing_action_is_warning(self) -> None:
        screen = good_screen()
        del screen["elements"][0]["elements"][1]["action"]
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "focus_visibility" and "no action" in f.message for f in findings))

    def test_hover_defined_passes(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertFalse(any(f.check == "focus_visibility" and f.severity == "error" for f in findings))


class AlignmentTests(unittest.TestCase):
    def test_center_anchor_with_non_05_x_is_warning(self) -> None:
        screen = good_screen()
        screen["elements"][0]["x"] = 0.4  # centre anchor should use x=0.5
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "alignment" and "expects x" in f.message for f in findings))

    def test_y_outside_unit_range_is_warning(self) -> None:
        screen = good_screen()
        screen["elements"][0]["elements"][0]["y"] = 3.0
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "alignment" and "outside the 0..1" in f.message for f in findings))


class ClippingTests(unittest.TestCase):
    def test_child_escaping_panel_is_error(self) -> None:
        screen = good_screen()
        # Push a button to the far edge so its rect escapes the 480x520 panel.
        screen["elements"][0]["elements"].append(
            {
                "type": "button",
                "label": "EDGE",
                "style": "secondary",
                "action": "pop_screen",
                "x": 0.98,
                "y": 0.9,
                "anchor": "top_left",
            }
        )
        menus = make_menus_dir(THEME, {"main_menu": screen})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertTrue(any(f.check == "clipping" and f.severity == "error" for f in findings))

    def test_well_fit_screen_has_no_clipping_error(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
        finally:
            cleanup_menus_dir(menus)
        self.assertFalse(any(f.check == "clipping" and f.severity == "error" for f in findings))


class ReportTests(unittest.TestCase):
    def test_markdown_report_written(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
            with tempfile.TemporaryDirectory() as out_dir:
                report = Path(out_dir) / "report.md"
                checker._write_markdown(
                    report, findings, {"screens_in_scope": ["main_menu"], "supported_resolutions": [[1280, 720]]}
                )
                self.assertTrue(report.is_file())
                text = report.read_text(encoding="utf-8")
                self.assertIn("Flashback menu visual QA checklist", text)
        finally:
            cleanup_menus_dir(menus)

    def test_json_serializable(self) -> None:
        menus = make_menus_dir(THEME, {"main_menu": good_screen()})
        try:
            checker_inst = checker.MenuQaChecker(menus, {"screens_in_scope": ["main_menu"]}, [[1280, 720]])
            findings = checker_inst.run()
            json.dumps(checker._findings_to_dicts(findings))
        finally:
            cleanup_menus_dir(menus)


if __name__ == "__main__":
    unittest.main()

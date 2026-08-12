#!/usr/bin/env python3
"""Static visual QA checklist for Flashback menu screens (dev-only).

This tool validates the JSON menu definitions in ``assets/menus`` against a
repeatable visual QA checklist covering:

* **Structure** — theme and scoped screens exist and parse.
* **Typography orientation** — text elements reference a defined font, use a
  positive scale, carry non-empty content, and stay inside their parent.
* **Contrast** — WCAG 2.x relative-luminance contrast between text colors and
  the effective background (panel or screen background).
* **Focus visibility** — interactive elements expose a labelled action and a
  hover colour is defined for every button style in the theme.
* **Alignment** — centre-anchored elements are actually centred and element
  anchors stay within the parent.
* **Clipping** — every element rect (computed with the same layout math as
  ``engine/ui/src/menu_system.cpp``) fits inside its parent and the screen at
  every supported resolution.

The tool is development-only: it is not part of any game build and does not
enable the game MCP bridge.

Usage::

    python3 tools/menu_qa/menu_qa_check.py [--menus-dir assets/menus]
        [--config tools/menu_qa/menu_qa_config.json]
        [--json findings.json] [--markdown report.md]

Exit code is 0 when only warnings were produced, 1 when errors were produced.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

DEFAULT_CONFIG = Path(__file__).resolve().parent / "menu_qa_config.json"

# Element types that can receive focus / need an action.
INTERACTIVE_TYPES = {"button", "map_card", "slider", "toggle"}
# Element types that produce a drawable rect used for clipping checks.
RECT_TYPES = {"panel", "text", "button", "map_card", "slider", "toggle", "progress_bar", "spinner"}


@dataclass
class Finding:
    check: str
    severity: str  # "error" | "warning"
    screen: str
    element: str  # human readable element path, e.g. "elements[1].elements[2]"
    message: str
    resolution: str = ""  # "1280x720" when resolution-specific


@dataclass
class Rect:
    x: float
    y: float
    w: float
    h: float

    def contains(self, other: Rect, tolerance: float = 0.0) -> bool:
        return (
            other.x >= self.x - tolerance
            and other.y >= self.y - tolerance
            and other.x + other.w <= self.x + self.w + tolerance
            and other.y + other.h <= self.y + self.h + tolerance
        )


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_color(theme_colors: dict[str, list[float]], name: str) -> list[float]:
    """Resolve a theme colour name to an RGBA float list, defaulting to white."""
    if name in theme_colors:
        return theme_colors[name]
    return [1.0, 1.0, 1.0, 1.0]


def srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4


def relative_luminance(rgba: list[float]) -> float:
    r, g, b = (srgb_to_linear(max(0.0, min(1.0, rgba[i]))) for i in range(3))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast_ratio(fg: list[float], bg: list[float]) -> float:
    l1 = relative_luminance(fg)
    l2 = relative_luminance(bg)
    if l1 < l2:
        l1, l2 = l2, l1
    return (l1 + 0.05) / (l2 + 0.05)


def compute_position(
    el_x: float,
    el_y: float,
    anchor: str,
    parent_x: float,
    parent_y: float,
    parent_w: float,
    parent_h: float,
    el_w: float,
    el_h: float,
) -> tuple[float, float]:
    """Mirror of ``MenuSystem::compute_position`` in menu_system.cpp."""
    base_x = parent_x + el_x
    base_y = parent_y + el_y

    if anchor == "center":
        return base_x - el_w * 0.5, base_y - el_h * 0.5
    if anchor == "top_center":
        return base_x - el_w * 0.5, base_y
    if anchor == "bottom_center":
        return base_x - el_w * 0.5, base_y - el_h
    if anchor == "top_left":
        return base_x, base_y
    if anchor == "top_right":
        return base_x - el_w, base_y
    return base_x, base_y


class MenuQaChecker:
    """Runs the menu QA checklist over a menus directory."""

    def __init__(
        self,
        menus_dir: Path,
        config: dict[str, Any] | None = None,
        resolutions: list[list[int]] | None = None,
    ) -> None:
        self.menus_dir = menus_dir
        self.config = config or {}
        self.findings: list[Finding] = []
        checks = self.config.get("checks", {})
        self.contrast_normal = float(checks.get("contrast_normal_threshold", 4.5))
        self.contrast_large = float(checks.get("contrast_large_threshold", 3.0))
        self.large_text_scale = float(checks.get("large_text_font_scale", 1.6))
        self.alignment_tolerance = float(checks.get("alignment_x_tolerance", 0.01))
        self.font_base_size = float(checks.get("font_base_size_px", 20.0))
        self.char_width = float(checks.get("char_width_estimate_px", 8.0))
        self.resolutions = resolutions or config.get("supported_resolutions", [[1280, 720]])
        self.screens_in_scope = config.get("screens_in_scope", [])
        self.theme: dict[str, Any] = {}
        self.screens: dict[str, dict[str, Any]] = {}

    # ── Helpers ──────────────────────────────────────────────────────────────

    def _add(
        self,
        check: str,
        severity: str,
        screen: str,
        element: str,
        message: str,
        resolution: str = "",
    ) -> None:
        self.findings.append(
            Finding(
                check=check, severity=severity, screen=screen, element=element, message=message, resolution=resolution
            )
        )

    def _element_path(self, indexes: list[int]) -> str:
        parts = []
        for i, idx in enumerate(indexes):
            parts.append(f"elements[{idx}]" if i == 0 else f".elements[{idx}]")
        return "".join(parts)

    def _effective_background(self, screen: dict[str, Any], parent_chain: list[dict[str, Any]]) -> list[float]:
        """Background behind an element: nearest coloured panel, else screen bg,
        else the theme 'bg' colour."""
        theme_colors = self.theme.get("colors", {})
        for parent in reversed(parent_chain):
            if parent.get("type") == "panel":
                color_name = parent.get("color", "none")
                if color_name != "none":
                    return resolve_color(theme_colors, color_name)
        bg = screen.get("background", [])
        if len(bg) >= 3:
            return bg
        if "bg" in theme_colors:
            return theme_colors["bg"]
        return [0.0, 0.0, 0.0, 1.0]

    def _element_size(
        self,
        el: dict[str, Any],
        font_scale: float = 1.0,
        text: str = "",
        theme_spacing: dict[str, float] | None = None,
    ) -> tuple[float, float]:
        """Estimate the drawable size of an element (scale = 1.0)."""
        spacing = theme_spacing or {}
        etype = el.get("type", "panel")
        width = float(el.get("width", 0) or 0)
        height = float(el.get("height", 0) or 0)

        if etype == "panel":
            return max(width, 0.0), max(height, 0.0)
        if etype == "button":
            return float(spacing.get("button_width", 280)), float(spacing.get("button_height", 48))
        if etype == "text":
            return len(text) * self.char_width * font_scale, font_scale * self.font_base_size
        if etype == "map_card":
            return max(width, 0.0), max(height, 0.0)
        if etype == "slider":
            return (width if width > 0 else 300.0), 40.0
        if etype == "toggle":
            return (width if width > 0 else 300.0), 30.0
        if etype == "progress_bar":
            return (width if width > 0 else 300.0), (height if height > 0 else 8.0)
        if etype == "spinner":
            r = float(el.get("radius", 0) or 0)
            if r <= 0:
                r = 10.0
            return 2 * r, 2 * r
        return max(width, 0.0), max(height, 0.0)

    # ── Structure ────────────────────────────────────────────────────────────

    def check_structure(self) -> None:
        theme_path = self.menus_dir / "theme.json"
        if not theme_path.is_file():
            self._add("structure", "error", "*", "-", f"Missing theme file {theme_path}")
            return
        try:
            self.theme = load_json(theme_path)
        except json.JSONDecodeError as exc:
            self._add("structure", "error", "*", "theme.json", f"theme.json is not valid JSON: {exc}")
            return

        for name in self.screens_in_scope:
            screen_path = self.menus_dir / f"{name}.json"
            if not screen_path.is_file():
                self._add("structure", "error", name, "-", f"Scoped screen {name}.json is missing")
                continue
            try:
                self.screens[name] = load_json(screen_path)
            except json.JSONDecodeError as exc:
                self._add("structure", "error", name, "-", f"{name}.json is not valid JSON: {exc}")

    # ── Typography ───────────────────────────────────────────────────────────

    def check_typography(self) -> None:
        fonts = self.theme.get("fonts", {})
        for screen_name, screen in self.screens.items():
            self._walk(screen_name, screen.get("elements", []), [], fonts)

    def _walk(
        self,
        screen_name: str,
        elements: list[dict[str, Any]],
        indexes: list[int],
        fonts: dict[str, Any],
    ) -> None:
        theme_colors = self.theme.get("colors", {})
        for i, el in enumerate(elements):
            path_indexes = [*indexes, i]
            path = self._element_path(path_indexes)
            etype = el.get("type", "panel")

            if etype == "text":
                font_name = el.get("font", "")
                font_scale = 1.0
                if font_name:
                    if font_name not in fonts:
                        self._add(
                            "typography",
                            "error",
                            screen_name,
                            path,
                            f"Text references unknown font '{font_name}' (theme fonts: {sorted(fonts)})",
                        )
                    else:
                        font_scale = float(fonts[font_name])
                if font_scale <= 0:
                    self._add(
                        "typography", "error", screen_name, path, f"Font scale must be positive, got {font_scale}"
                    )
                content = str(el.get("content", "")).strip()
                if not content:
                    self._add("typography", "warning", screen_name, path, "Text element has empty content")
                # Colour must resolve to a theme token when one is requested.
                color_name = el.get("color", "")
                if color_name and color_name not in theme_colors:
                    self._add(
                        "typography",
                        "warning",
                        screen_name,
                        path,
                        f"Text references unknown colour '{color_name}' (defaults to white)",
                    )

            if el.get("elements"):
                self._walk(screen_name, el["elements"], path_indexes, fonts)

    # ── Contrast ─────────────────────────────────────────────────────────────

    def check_contrast(self) -> None:
        for screen_name, screen in self.screens.items():
            self._contrast_walk(screen_name, screen, screen.get("elements", []), [], [])

    def _contrast_walk(
        self,
        screen_name: str,
        screen: dict[str, Any],
        elements: list[dict[str, Any]],
        indexes: list[int],
        parent_chain: list[dict[str, Any]],
    ) -> None:
        theme_colors = self.theme.get("colors", {})
        for i, el in enumerate(elements):
            path_indexes = [*indexes, i]
            path = self._element_path(path_indexes)
            etype = el.get("type", "panel")

            if etype == "panel":
                parent_chain = [*parent_chain, el]
                if el.get("elements"):
                    self._contrast_walk(screen_name, screen, el["elements"], path_indexes, parent_chain)
                continue

            bg = self._effective_background(screen, parent_chain)

            if etype == "text":
                color = resolve_color(theme_colors, el.get("color", ""))
                font_name = el.get("font", "")
                font_scale = float(self.theme.get("fonts", {}).get(font_name, 1.0)) if font_name else 1.0
                threshold = self.contrast_normal
                if font_scale >= self.large_text_scale:
                    threshold = self.contrast_large
                ratio = contrast_ratio(color, bg)
                if ratio < threshold:
                    self._add(
                        "contrast",
                        "error",
                        screen_name,
                        path,
                        f"Contrast {ratio:.2f}:1 is below WCAG threshold {threshold}:1 (fg={color}, bg={bg})",
                    )
            elif etype == "button":
                # Button label text renders in the default text colour on the
                # button fill; check the fill against the surrounding background
                # so the control is at least distinguishable.
                style = el.get("style", "")
                button_bg = resolve_color(theme_colors, f"button_{style}" if style else "button_primary")
                ratio = contrast_ratio(button_bg, bg)
                if ratio < self.contrast_large:
                    self._add(
                        "contrast",
                        "warning",
                        screen_name,
                        path,
                        f"Button fill contrast {ratio:.2f}:1 vs background is below {self.contrast_large}:1",
                    )

    # ── Focus visibility ─────────────────────────────────────────────────────

    def check_focus_visibility(self) -> None:
        theme_colors = self.theme.get("colors", {})
        known_actions = {
            "push_screen",
            "pop_screen",
            "pop_to_root",
            "start_game",
            "start_sandbox",
            "resume_game",
            "quit_application",
            "setting_changed",
            "apply_settings",
        }
        for screen_name, screen in self.screens.items():
            self._focus_walk(screen_name, screen.get("elements", []), [], theme_colors, known_actions)

    def _focus_walk(
        self,
        screen_name: str,
        elements: list[dict[str, Any]],
        indexes: list[int],
        theme_colors: dict[str, Any],
        known_actions: set[str],
    ) -> None:
        for i, el in enumerate(elements):
            path_indexes = [*indexes, i]
            path = self._element_path(path_indexes)
            etype = el.get("type", "panel")

            if etype in INTERACTIVE_TYPES:
                label = str(el.get("label", "")).strip()
                action = str(el.get("action", "")).strip()
                if not label:
                    self._add("focus_visibility", "warning", screen_name, path, f"{etype} has no label")
                if not action:
                    self._add(
                        "focus_visibility",
                        "warning",
                        screen_name,
                        path,
                        f"{etype} has no action (focus cannot trigger a result)",
                    )
                elif etype == "button":
                    style = el.get("style", "")
                    hover_key = f"button_{style}_hover" if style else "button_primary_hover"
                    if hover_key not in theme_colors:
                        self._add(
                            "focus_visibility",
                            "error",
                            screen_name,
                            path,
                            f"Button style '{style or 'primary'}' has no hover colour '{hover_key}' in theme — "
                            "hover/focus state is not visible",
                        )

            if el.get("elements"):
                self._focus_walk(screen_name, el["elements"], path_indexes, theme_colors, known_actions)

    # ── Alignment ────────────────────────────────────────────────────────────

    def check_alignment(self) -> None:
        for screen_name, screen in self.screens.items():
            self._alignment_walk(screen_name, screen, screen.get("elements", []), [], 0.0, 0.0, 1.0, 1.0, 0.0, 0.0)

    def _alignment_walk(
        self,
        screen_name: str,
        screen: dict[str, Any],
        elements: list[dict[str, Any]],
        indexes: list[int],
        parent_x: float,
        parent_y: float,
        parent_w: float,
        parent_h: float,
        offset_x: float,
        offset_y: float,
    ) -> None:
        theme_spacing = self.theme.get("spacing", {})
        for i, el in enumerate(elements):
            path_indexes = [*indexes, i]
            path = self._element_path(path_indexes)
            etype = el.get("type", "panel")
            anchor = el.get("anchor", "")

            if anchor in {"center", "top_center", "bottom_center"}:
                x = float(el.get("x", 0) or 0)
                if abs(x - 0.5) > self.alignment_tolerance:
                    self._add(
                        "alignment",
                        "warning",
                        screen_name,
                        path,
                        f"'{anchor}' anchor expects x≈0.5 but got x={x}",
                    )

            # Vertical bounds for fractional coordinates.
            y = float(el.get("y", 0) or 0)
            if not (0.0 <= y <= 1.0):
                self._add(
                    "alignment",
                    "warning",
                    screen_name,
                    path,
                    f"y={y} is outside the 0..1 parent-relative range (may render off-screen)",
                )

            if etype == "panel":
                text = ""
                font_scale = 1.0
                w, h = self._element_size(el, font_scale, text, theme_spacing)
                pos = compute_position(
                    float(el.get("x", 0) or 0) * parent_w,
                    float(el.get("y", 0) or 0) * parent_h,
                    anchor,
                    offset_x,
                    offset_y,
                    parent_w,
                    parent_h,
                    w,
                    h,
                )
                if el.get("elements"):
                    self._alignment_walk(
                        screen_name,
                        screen,
                        el["elements"],
                        path_indexes,
                        w,
                        h,
                        pos[0],
                        pos[1],
                        pos[0],
                        pos[1],
                    )

    # ── Clipping ─────────────────────────────────────────────────────────────

    def check_clipping(self) -> None:
        theme_spacing = self.theme.get("spacing", {})
        for screen_name, screen in self.screens.items():
            for width, height in self.resolutions:
                screen_rect = Rect(0, 0, float(width), float(height))
                self._clipping_walk(
                    screen_name,
                    screen,
                    screen.get("elements", []),
                    [],
                    screen_rect,
                    theme_spacing,
                    f"{width}x{height}",
                )

    def _clipping_walk(
        self,
        screen_name: str,
        screen: dict[str, Any],
        elements: list[dict[str, Any]],
        indexes: list[int],
        parent_rect: Rect,
        theme_spacing: dict[str, float],
        resolution: str,
    ) -> None:
        for i, el in enumerate(elements):
            path_indexes = [*indexes, i]
            path = self._element_path(path_indexes)
            etype = el.get("type", "panel")
            anchor = el.get("anchor", "")

            if etype in {"spacer", "separator"}:
                continue

            text = str(el.get("content", "")).strip()
            font_name = el.get("font", "")
            font_scale = float(self.theme.get("fonts", {}).get(font_name, 1.0)) if font_name else 1.0
            w, h = self._element_size(el, font_scale, text, theme_spacing)
            if w <= 0 or h <= 0:
                continue

            pos = compute_position(
                float(el.get("x", 0) or 0) * parent_rect.w,
                float(el.get("y", 0) or 0) * parent_rect.h,
                anchor,
                parent_rect.x,
                parent_rect.y,
                parent_rect.w,
                parent_rect.h,
                w,
                h,
            )
            el_rect = Rect(pos[0], pos[1], w, h)

            if not parent_rect.contains(el_rect):
                self._add(
                    "clipping",
                    "error",
                    screen_name,
                    path,
                    f"{etype} rect ({pos[0]:.0f},{pos[1]:.0f} {w:.0f}x{h:.0f}) "
                    f"escapes parent rect ({parent_rect.x:.0f},{parent_rect.y:.0f} "
                    f"{parent_rect.w:.0f}x{parent_rect.h:.0f})",
                    resolution,
                )

            if etype == "panel" and el.get("elements"):
                self._clipping_walk(
                    screen_name, screen, el["elements"], path_indexes, el_rect, theme_spacing, resolution
                )

    # ── Orchestration ────────────────────────────────────────────────────────

    def run(self) -> list[Finding]:
        self.check_structure()
        if not self.screens:
            return self.findings
        self.check_typography()
        self.check_contrast()
        self.check_focus_visibility()
        self.check_alignment()
        self.check_clipping()
        return self.findings


def _findings_to_dicts(findings: list[Finding]) -> list[dict[str, str]]:
    return [
        {
            "check": f.check,
            "severity": f.severity,
            "screen": f.screen,
            "element": f.element,
            "message": f.message,
            "resolution": f.resolution,
        }
        for f in findings
    ]


def _write_markdown(path: Path, findings: list[Finding], config: dict[str, Any]) -> None:
    lines = ["# Flashback menu visual QA checklist", ""]
    lines.append("Dev-only repeatable pass. Static checks run against the menu JSON;")
    lines.append("screenshot/log capture is driven separately by `tools/menu_qa/run_menu_qa.sh`.")
    lines.append("")
    lines.append(f"Scope screens: {', '.join(config.get('screens_in_scope', []))}")
    lines.append(f"Supported resolutions: {', '.join(f'{w}x{h}' for w, h in config.get('supported_resolutions', []))}")
    lines.append("")

    errors = [f for f in findings if f.severity == "error"]
    warnings = [f for f in findings if f.severity == "warning"]
    lines.append(f"**Result:** {len(errors)} error(s), {len(warnings)} warning(s)")
    lines.append("")

    if errors or warnings:
        lines.append("## Findings")
        lines.append("")
        lines.append("| Check | Severity | Screen | Element | Resolution | Message |")
        lines.append("| --- | --- | --- | --- | --- | --- |")
        for f in findings:
            safe = f.message.replace("|", "\\|")
            lines.append(f"| {f.check} | {f.severity} | {f.screen} | {f.element} | {f.resolution} | {safe} |")
        lines.append("")

    lines.append("## Manual visual checks (screenshot capture)")
    lines.append("")
    for screen in config.get("screens_in_scope", []):
        lines.append(f"- [ ] `{screen}` — typography orientation, contrast, focus visibility, alignment, clipping")
    lines.append("")
    lines.append("## Launch / crash log recording")
    lines.append("")
    lines.append("- Launch logs, crash dumps, and game logs are stored separately from visual findings")
    lines.append("  under the QA output directory (`build/menu_qa/<timestamp>/logs/`).")
    lines.append("- Visual defects are reported with a screenshot path and the matching log excerpt.")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--menus-dir", type=Path, default=None, help="Path to assets/menus")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG, help="QA config JSON")
    parser.add_argument("--json", type=Path, default=None, help="Write findings as JSON to this path")
    parser.add_argument("--markdown", type=Path, default=None, help="Write a markdown report to this path")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    config = {}
    if args.config and args.config.is_file():
        config = load_json(args.config)
    else:
        print(f"Warning: config {args.config} not found; using defaults.", file=sys.stderr)

    menus_dir = args.menus_dir
    if menus_dir is None:
        repo_root = Path(__file__).resolve().parents[2]
        menus_dir = repo_root / "assets" / "menus"

    checker = MenuQaChecker(menus_dir, config)
    findings = checker.run()

    for f in findings:
        severity = f.severity.upper()
        where = f"{f.screen} {f.element}"
        if f.resolution:
            where += f" @ {f.resolution}"
        print(f"[{severity}] {f.check} | {where} | {f.message}")

    if args.json:
        args.json.write_text(
            json.dumps(_findings_to_dicts(findings), indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    if args.markdown:
        _write_markdown(args.markdown, findings, config)

    print(
        f"\n{len(findings)} finding(s) — {sum(1 for f in findings if f.severity == 'error')} error(s), "
        f"{sum(1 for f in findings if f.severity == 'warning')} warning(s)."
    )

    if any(f.severity == "error" for f in findings):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

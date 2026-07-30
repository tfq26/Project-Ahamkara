#!/usr/bin/env python3
"""Validate docs viewer JSON files have correct structure and syntax."""

import json
import os
import sys


def main() -> int:
    repo_root = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
    errors = []

    # 1. docs_viewer.json exists and is valid JSON
    docs_viewer_path = os.path.join(repo_root, "assets", "menus", "docs_viewer.json")
    try:
        with open(docs_viewer_path) as f:
            screen = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        errors.append(f"docs_viewer.json: {e}")
        screen = {}

    # 2. Screen has a docs_viewer element in the content panel
    if screen:
        panels = screen.get("elements", [])
        if not panels:
            errors.append("docs_viewer.json: no top-level elements")
        else:
            outer_panel = panels[0]
            inner_elements = outer_panel.get("elements", [])
            dv_elements = [e for e in inner_elements if e.get("type") == "docs_viewer"]
            if not dv_elements:
                errors.append("docs_viewer.json: no docs_viewer element")
            else:
                dv = dv_elements[0]
                sections = dv.get("elements", [])
                section_ids = [s.get("id", "") for s in sections]
                section_labels = [s.get("label", "") for s in sections]
                expected_sections = ["architecture", "api_reference", "deployment_guide"]
                expected_labels = ["Architecture", "API Reference", "Deployment Guide"]

                # Verify all required sections exist
                for sid in expected_sections:
                    if sid not in section_ids:
                        errors.append(f"docs_viewer.json: missing section id '{sid}'")
                for slbl in expected_labels:
                    if slbl not in section_labels:
                        errors.append(f"docs_viewer.json: missing section label '{slbl}'")

                # Verify code blocks exist
                code_block_count = 0
                for section in sections:
                    for child in section.get("elements", []):
                        if child.get("type") == "code_block":
                            code_block_count += 1
                            if not child.get("content", "").strip():
                                errors.append(f"docs_viewer.json: section '{section.get('id')}' has empty code_block")
                if code_block_count == 0:
                    errors.append("docs_viewer.json: no code_block elements found")

    # 3. pause_menu.json has DOCUMENTATION button
    pause_path = os.path.join(repo_root, "assets", "menus", "pause_menu.json")
    try:
        with open(pause_path) as f:
            pause = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError) as e:
        errors.append(f"pause_menu.json: {e}")
        pause = {}

    if pause:
        elements = pause.get("elements", [])
        buttons = []
        def collect_buttons(el_list):
            for el in el_list:
                if el.get("type") == "panel":
                    collect_buttons(el.get("elements", []))
                elif el.get("type") == "button":
                    buttons.append(el.get("label", ""))
        collect_buttons(elements)
        if "DOCUMENTATION" not in buttons:
            errors.append("pause_menu.json: missing 'DOCUMENTATION' button")

    for err in errors:
        print(f"FAIL: {err}")

    if errors:
        print(f"\n{len(errors)} validation error(s) found.")
        return 1

    print("All docs viewer validations passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

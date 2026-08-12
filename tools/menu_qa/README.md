# Flashback menu visual QA checklist (dev-only)

A repeatable visual QA pass for Flashback menu screens, run after every menu
change during local development. It is intentionally **not** part of any game
build and never enables the game MCP bridge.

## Checklist scope

| Screen | JSON definition | State |
| --- | --- | --- |
| Main menu | `assets/menus/main_menu.json` | Startup |
| Settings | `assets/menus/settings.json` | From main menu / pause |
| Pause | `assets/menus/pause_menu.json` | In-game pause overlay |
| Loading / transition | `assets/menus/loading_screen.json` | Map load transition |

Supported resolutions: `1280x720`, `1600x900`, `1920x1080`, `2560x1440`
(matching the client settings list in `engine/ui/src/ahamkara_ui.cpp`).

## Checks performed

1. **Structure** — the theme file and every scoped screen parse as JSON and
   exist.
2. **Typography orientation** — text elements reference a defined font, use a
   positive scale, carry non-empty content, and stay inside their parent.
3. **Contrast** — WCAG 2.x relative-luminance contrast between text colours and
   the effective background (panel or screen background). Thresholds: `4.5:1`
   normal text, `3.0:1` large text / button fills.
4. **Focus visibility** — interactive elements expose a labelled action and
   every button style has a hover colour in the theme.
5. **Alignment** — centre-anchored elements are actually centred (`x ≈ 0.5`)
   and fractional `y` coordinates stay within `0..1`.
6. **Clipping** — every element rect (computed with the same layout math as
   `engine/ui/src/menu_system.cpp`) fits inside its parent and the screen at
   every supported resolution.

## Run the checklist

```sh
# Static checks only (no display required)
python3 tools/menu_qa/menu_qa_check.py

# Full dev pass: build flashback, static checks + screenshot capture + logs
tools/menu_qa/run_menu_qa.sh --capture

# Capture a single resolution
tools/menu_qa/run_menu_qa.sh --capture --resolution 1920x1080
```

Output lands in `build/menu_qa/<timestamp>/`:

| Path | Contents |
| --- | --- |
| `findings.json` | Machine-readable findings |
| `report.md` | Human-readable checklist report |
| `screenshots/menu-<res>.png` | Captured menu frames (when `--capture`) |
| `logs/launch-<res>.log` | Launch output, recorded separately |
| `launch-issues.txt` | Crash/error log excerpts |

## Interpreting failures

Every failure is actionable: it names the screen, the element path, the
resolution, and the expected vs. actual geometry. A screenshot and the matching
launch-log excerpt are written next to the findings so a menu change can be
confirmed visually or reverted quickly.

Example:

```
[ERROR] clipping | main_menu elements[0].elements[2] @ 1280x720 |
        button rect (500,93340 280x48) escapes parent rect (400,100 480x520)
```

`run_menu_qa.sh` records launch and crash logs under `logs/` separately from
the visual findings, and greps them for crash indicators (`error`, `crash`,
`fatal`, `segv`, `abort`, `assert`) so infrastructure failures are never mixed
with visual defects.

## Notes

- Screenshot capture uses `screencapture` (macOS), `import` (ImageMagick), or
  `gnome-screenshot` (Linux) whichever is available.
- The flashback binary accepts `--window-width` and `--window-height` so each
  supported resolution can be exercised.
- This tool is dev-only: it is not compiled into the client and it does not
  enable or expose the game MCP bridge.

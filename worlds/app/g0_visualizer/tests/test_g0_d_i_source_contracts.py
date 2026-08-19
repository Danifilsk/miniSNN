#!/usr/bin/env python3
"""Focused source contracts for G0-D-I startup and Settings geometry."""

import argparse
from pathlib import Path


def body(source: str, declaration: str) -> str:
    start = source.find(declaration)
    if start < 0:
        raise AssertionError(f"missing function: {declaration}")
    opening = source.find("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise AssertionError(f"unterminated function: {declaration}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--window-source", required=True)
    parser.add_argument("--layout-source", required=True)
    args = parser.parse_args()

    window = Path(args.window_source).read_text(encoding="ascii")
    layout = Path(args.layout_source).read_text(encoding="ascii")

    assert "GetModuleFileNameW" in window
    assert "executable_directory_utf8" in window
    assert "asset_root_has_assets_directory" in window
    resolver = body(window, "static int resolve_repository_root_from_executable(")
    assert '"."' in resolver
    assert "GetCurrentDirectory" not in resolver
    assert "ASSET INITIALIZATION FAILED" in window
    assert "SANDBOX INITIALIZATION FAILED" in window
    assert "OutputDebugStringA" in window
    assert '"--startup-smoke"' in window
    for line in (
        "executable_root=%s",
        "assets_found=yes",
        "assets_initialized=yes",
        "sandbox_initialized=yes",
        "startup=PASS",
    ):
        assert line in window

    font = body(window, "static void create_ui_font(")
    button = body(window, "static void draw_button(")
    settings_start = window.index("static void draw_settings_panel(HDC hdc, const RECT *client)" + chr(10) + "{")
    settings = body(window[settings_start:], "static void draw_settings_panel(")
    assert '"Fixedsys"' in font
    assert "CreateFontA(-height, width" in font
    assert '"Terminal"' not in font
    assert "SYSTEM_FIXED_FONT" in font
    assert "SelectObject(hdc, g_app.ui_font.handle)" in button
    assert "GetTextExtentPoint32A" in button
    assert "g0_visualizer_settings_layout" in settings
    assert "const int height" not in settings
    assert "G0_VISUALIZER_SETTINGS_BACK" in settings

    assert "g0_visualizer_settings_layout" in layout
    assert "g0_visualizer_settings_layout_is_valid" in layout
    assert "G0_VISUALIZER_SETTINGS_CONTROL_COUNT" in layout
    assert "minimum_bottom_padding" in layout
    print("G0-D-I startup and Settings source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
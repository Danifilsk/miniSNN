#!/usr/bin/env python3
"""Focused source contracts for G0-D-H presentation and blueprint resize."""

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
    parser.add_argument("--settings-source", required=True)
    parser.add_argument("--document-source", required=True)
    parser.add_argument("--config-source", required=True)
    parser.add_argument("--runtime-source", required=True)
    parser.add_argument("--layout-source", required=True)
    args = parser.parse_args()

    window = Path(args.window_source).read_text(encoding="ascii")
    settings = Path(args.settings_source).read_text(encoding="ascii")
    document = Path(args.document_source).read_text(encoding="ascii")
    config = Path(args.config_source).read_text(encoding="ascii")
    runtime = Path(args.runtime_source).read_text(encoding="ascii")
    layout = Path(args.layout_source).read_text(encoding="ascii")

    assert "NONANTIALIASED_QUALITY" in window
    assert '"Fixedsys"' in window
    assert "SYSTEM_FIXED_FONT" in window
    assert "CreateFontA(-height, width" in window
    assert '"Terminal"' not in window
    assert "g0_app_settings_effective_water_alpha" in window
    assert "resize_editor_world" in window
    assert "g0_visualizer_runtime_resize_world" in window
    assert "G0_WORLD_DOCUMENT_FORMAT_V1" in document
    assert "water_opacity_percent" in document
    assert "presentation_mode" in document
    assert "g0_world_config_resize" in config
    assert "g0_world_config_resize" in runtime
    assert "g0_visualizer_header_control_layout" in layout
    assert "g0_visualizer_header_controls_are_disjoint" in layout
    assert "g0_visualizer_settings_layout" in layout
    assert "g0_visualizer_settings_layout_is_valid" in layout
    assert "g0_visualizer_settings_layout" in window
    assert 'strcmp(token, "--runtime-smoke")' in window
    assert "world_config_format=V2" in window
    assert "water_opacity_setting=yes" in window
    assert "presentation_mode=yes" in window
    assert "transactional_resize=yes" in window

    editor_click = body(window, "static void handle_editor_click(")
    resize = body(window, "static void resize_editor_world(")
    new_world = body(window, "static void new_editor_world(")
    dialog = body(window, "static LRESULT CALLBACK new_world_dialog_proc(")

    assert editor_click.count("resize_editor_world") == 4
    assert "new_editor_world" in editor_click
    assert "g0_visualizer_runtime_resize_world" in resize
    assert "g0_visualizer_runtime_new_world" in new_world
    assert "g0_visualizer_runtime_new_world" not in resize

    assert "G0_NEW_WORLD_CANCEL" in dialog
    assert "DestroyWindow(hwnd)" in dialog
    assert "EnableWindow(g_app.main_window, TRUE)" in dialog
    assert "DestroyWindow(g_app.main_window)" not in dialog
    assert "PostQuitMessage" not in dialog
    assert "g_app.app_state" not in dialog
    print("G0-D-H presentation and resize source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

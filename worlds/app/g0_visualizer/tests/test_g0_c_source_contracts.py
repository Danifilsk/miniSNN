#!/usr/bin/env python3
"""Focused source contracts for the G0-C interactive Win32 sandbox."""

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
    parser.add_argument("--runtime-source", required=True)
    parser.add_argument("--runtime-header", required=True)
    args = parser.parse_args()

    window = Path(args.window_source).read_text(encoding="ascii")
    runtime = Path(args.runtime_source).read_text(encoding="ascii")
    header = Path(args.runtime_header).read_text(encoding="ascii")
    paint = body(window, "static LRESULT CALLBACK sandbox_window_proc(")
    apply = body(window, "static int apply_editor_world(")
    base = body(window, "static void draw_base_terrain(")
    water = body(window, "static void draw_water_overlay(")
    scene = body(window, "static void draw_scene(")
    debug = body(window, "static void draw_debug_panel(")

    assert "CreateCompatibleDC" in window
    assert "CreateCompatibleBitmap" in window
    assert "WM_ERASEBKGND" in paint and "return 1;" in paint
    assert "BitBlt(screen" in paint
    assert "draw_scene(g_app.backbuffer_dc" in paint
    assert "g0_visualizer_runtime_create_sandbox" in window
    assert "Persistent sandbox for SNN minds" in window
    assert "Brain: WF0 Fixed Brain V1" in window
    assert "trainable_brain" not in runtime
    assert "G0_EDITOR_PANEL_WIDTH" in window
    assert "WATER" in window and "LAND" in window and "ROCK" in window
    assert "FOOD" in window and "ERASE" in window and "FISH SPAWN" in window
    assert "g0_visualizer_screen_to_tile" in window
    assert '#include "g0_visualizer_camera.h"' in window
    assert "g0_visualizer_camera_layout" in window
    assert "g0_visualizer_camera_pan_by_pixels" in window
    assert "g0_visualizer_camera_set_zoom_percent" in window
    assert "g_app.pan_x = g_app.map_origin_x - base_x" not in window
    assert "g0_visualizer_tile_to_screen_scaled" in base
    assert "g0_visualizer_tile_to_screen_scaled" in water
    assert "g0_visualizer_tile_to_screen(" not in window
    assert "G0_VISUALIZER_ASSET_DIRT" in base
    assert "G0_VISUALIZER_ASSET_WATER" in water
    assert "g0_app_settings_effective_water_alpha" in water
    assert scene.index("draw_base_terrain") < scene.index("draw_static_objects")
    assert scene.index("draw_static_objects") < scene.index("draw_runtime_food")
    assert scene.index("draw_runtime_food") < scene.index("draw_fish")
    assert scene.index("draw_fish") < scene.index("draw_water_overlay")
    assert scene.index("draw_water_overlay") < scene.index("draw_grid")
    assert "minisnn_worlds_terrain_world_to_tile" in window
    assert "Tile: (%u, %u)" in debug and "World: (%lld, %lld)" in debug
    assert "record->action_result.status" in debug
    assert "record->action_result.reason" in debug
    assert "GET_WHEEL_DELTA_WPARAM" in window
    assert "g0_visualizer_runtime_speed_increase" in window
    assert "g0_visualizer_runtime_step_once" in window
    assert "g0_visualizer_runtime_apply_config" in apply
    assert "runtime anterior foi preservado" in apply
    assert "g0_visualizer_runtime_prepare_edit" in window
    assert "g0_visualizer_runtime_new_world" in window
    assert "g0_visualizer_runtime_resize_world" in window
    assert "g0_visualizer_runtime_edit_tile" in window
    assert "g0_visualizer_runtime_create_sandbox" in header
    assert "g0_visualizer_runtime_apply_config" in header
    assert "g0_visualizer_tile_to_screen_scaled" in header
    assert "g0_visualizer_world_to_screen_scaled" in header
    assert "g0_visualizer_runtime_advance_elapsed" in runtime
    assert "g0_visualizer_tile_to_screen_scaled" in runtime
    print("G0-C Win32 sandbox source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

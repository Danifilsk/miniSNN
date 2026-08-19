#!/usr/bin/env python3
"""Focused source guard for G0-B timing, layout, and facing contracts."""

import argparse
from pathlib import Path


def function_body(source: str, declaration: str) -> str:
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

    window = Path(args.window_source).read_text(encoding="utf-8")
    runtime = Path(args.runtime_source).read_text(encoding="utf-8")
    header = Path(args.runtime_header).read_text(encoding="utf-8")
    timer = function_body(window, "static LRESULT CALLBACK g0_window_proc(")
    step_once = function_body(runtime, "int g0_visualizer_runtime_step_once(")

    assert "G0_VISUALIZER_TICKS_PER_SECOND 4U" in header
    assert "g0_visualizer_runtime_advance_elapsed" in timer
    assert "g0_visualizer_runtime_advance(g_app.runtime)" not in timer
    assert "RUNNING" in window and "PAUSED" in window and "ticks/s:" in window
    assert "G0_HEADER_HEIGHT" in window and "G0_DEBUG_PANEL_WIDTH" in window
    assert "fish_facing[4]" in window and "create_rotated_fish_bitmap" in window
    assert "g0_visualizer_runtime_facing" in window
    assert "record.action_result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED" in step_once
    assert "runtime->facing = g0_visualizer_facing_from_move_delta" in step_once
    assert "G0_VISUALIZER_FACING_SOUTH" in runtime
    print("G0-B Win32/runtime source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

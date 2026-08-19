#!/usr/bin/env python3
"""Small source-level guard for the Win32-only alpha and startup contracts."""

import argparse
from pathlib import Path


def draw_bitmap_body(source: str) -> str:
    start = source.find("static void draw_bitmap")
    end = source.find("static int tile_screen", start)
    if start < 0 or end < 0:
        raise AssertionError("draw_bitmap contract boundary missing")
    return source[start:end]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    args = parser.parse_args()
    source = Path(args.source).read_text(encoding="utf-8")
    body = draw_bitmap_body(source)

    assert "AlphaBlend" in body
    assert "StretchBlt" not in body
    assert "blend.BlendOp = AC_SRC_OVER" in body
    assert "blend.SourceConstantAlpha = alpha" in body
    assert "blend.AlphaFormat = AC_SRC_ALPHA" in body
    assert "resolve_repository_root_from_executable" in source
    assert "GetModuleFileNameA" in source
    assert "--runtime-smoke" in source

    terrain = source.find("draw_base_terrain(hdc")
    rocks = source.find("draw_rocks(hdc")
    entities = source.find("draw_entities(hdc")
    water = source.find("draw_water_overlay(hdc")
    interface = source.find("draw_ui(hdc")
    assert -1 not in (terrain, rocks, entities, water, interface)
    assert terrain < rocks < entities < water < interface
    assert "G0_VISUALIZER_ASSET_WATER" in source and "128U" in source
    print("G0-A Win32 source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

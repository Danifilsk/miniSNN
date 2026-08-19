#!/usr/bin/env python3
"""Focused source contracts for the G0-D product shell."""

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--window-source", required=True)
    parser.add_argument("--document-source", required=True)
    parser.add_argument("--log-source", required=True)
    args = parser.parse_args()

    window = Path(args.window_source).read_text(encoding="ascii")
    document = Path(args.document_source).read_text(encoding="ascii")
    log = Path(args.log_source).read_text(encoding="ascii")

    for state in ("G0_APP_MAIN_MENU", "G0_APP_WORLD_SANDBOX", "G0_APP_SETTINGS"):
        assert state in window
    for control in ("NEW WORLD", "LOAD WORLD", "OPEN SANDBOX", "SETTINGS", "SAVE", "LOAD"):
        assert control in window
    assert "confirm_save_or_discard" in window
    assert "MB_YESNOCANCEL" in window
    assert "EPISODE_RESET" in window and "next simulation tick is 1" in window
    assert "g0_visualizer_runtime_reset" in window
    assert "g0_world_document_load" in window
    assert "g0_visualizer_runtime_create_from_config" in window
    assert window.index("g0_world_document_load") < window.index("g0_visualizer_runtime_destroy(&g_app.runtime)")
    assert "worlds\\\\saves" in window
    assert "GetFullPathNameA" in window
    assert "Brain: WF0 Fixed Brain V1" in window
    assert "Neurons: 12 | Plasticity: OFF" in window
    assert "g0_event_log_export_csv" in window
    assert "G0_LOG_CATEGORY_SYSTEM" in window
    assert "G0_LOG_CATEGORY_ACTION" in window
    assert "G0_LOG_CATEGORY_FOOD" in window
    assert "G0_LOG_CATEGORY_ENERGY" in window
    assert "G0_LOG_CATEGORY_ERROR" in window
    assert "G0_WORLD_DOCUMENT_FORMAT" in document
    assert "MoveFileExA" in document
    assert "sequence,tick,category,actor,event,result,reason,text" in log
    print("G0-D product shell source contracts PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Static and artifact checks for the K2-C canonical command replay contract."""
from __future__ import annotations

import argparse
from pathlib import Path


RECORD_SIZE = 136
HEADER_SIZE = 40


def parse_key_values(path: Path) -> dict[str, str]:
    return dict(
        line.split("=", 1)
        for line in path.read_text(encoding="utf-8").splitlines()
        if "=" in line
    )


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise SystemExit(f"K2-C checker missing {label}: {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    header = root / "include" / "minisnn_worlds_kernel_command_log.h"
    source = root / "src" / "minisnn_worlds_kernel_command_log.c"
    app_source = root / "app" / "k2_command_log_file.c"
    contract = root / "docs" / "K2_C_COMMAND_REPLAY_CONTRACT.md"
    audit = root / "docs" / "K2_C_COMMAND_REPLAY_AUDIT.md"
    golden = Path(args.golden)
    for path, label in (
        (header, "public command-log header"),
        (source, "command-log source"),
        (app_source, "app file helper"),
        (contract, "contract"),
        (audit, "audit"),
        (golden, "golden"),
    ):
        require_file(path, label)

    header_text = header.read_text(encoding="utf-8")
    source_text = source.read_text(encoding="utf-8")
    app_text = app_source.read_text(encoding="utf-8")
    required_api = (
        "MINISNN_WORLDS_KERNEL_COMMAND_LOG_FORMAT_VERSION_V1",
        "minisnn_worlds_kernel_command_log_capture_submission",
        "minisnn_worlds_kernel_command_log_replay_next",
        "minisnn_worlds_kernel_command_log_from_bytes",
    )
    if any(token not in header_text for token in required_api):
        raise SystemExit("K2-C checker: public command-log API is incomplete")
    replay_tokens = (
        "submission_tick", "next_command_id", "REPLAY_DIVERGENCE",
        "queue_create_entity", "queue_destroy_entity", "queue_place_entity",
        "queue_remove_entity_from_space", "queue_set_occupancy",
        "queue_clear_occupancy", "queue_move_entity",
        "queue_create_spatial_link", "queue_remove_spatial_link",
    )
    if any(token not in source_text for token in replay_tokens):
        raise SystemExit("K2-C checker: replay does not cover the public command surface")
    if any(token in source_text for token in ("fopen(", "fwrite(", "fread(", "MoveFile")):
        raise SystemExit("K2-C checker: filesystem leaked into the Kernel library")
    if not all(token in app_text for token in (
        "fopen(", "fread(", "fwrite(", "MoveFileExA", "command_log_from_bytes",
    )):
        raise SystemExit("K2-C checker: app-layer command-log persistence is incomplete")

    directory = Path(args.directory)
    command_log = directory / "command_log.bin"
    snapshot = directory / "snapshot.bin"
    summary = directory / "summary.txt"
    for path, label in ((command_log, "command log"), (snapshot, "checkpoint snapshot"),
                        (summary, "demo summary")):
        require_file(path, label)
    data = command_log.read_bytes()
    if len(data) < HEADER_SIZE or data[:8] != b"MSWKLOG1":
        raise SystemExit("K2-C checker: command_log.bin magic is invalid")
    if int.from_bytes(data[8:12], "little") != 1 or int.from_bytes(data[12:16], "little") != 0:
        raise SystemExit("K2-C checker: command_log.bin version/reserved fields are invalid")
    count = int.from_bytes(data[16:24], "little")
    payload_size = int.from_bytes(data[24:32], "little")
    if payload_size != count * RECORD_SIZE or len(data) != HEADER_SIZE + payload_size:
        raise SystemExit("K2-C checker: command_log.bin size fields are invalid")
    snapshot_data = snapshot.read_bytes()
    if len(snapshot_data) < 40 or snapshot_data[:8] != b"MSWKSNP1":
        raise SystemExit("K2-C checker: checkpoint snapshot is not V1")

    observed = parse_key_values(summary)
    expected = parse_key_values(golden)
    if observed != expected:
        raise SystemExit(f"K2-C checker: golden mismatch: expected {expected}, got {observed}")
    if observed.get("final_original_hash") != observed.get("final_replay_hash"):
        raise SystemExit("K2-C checker: final replay hash diverges")
    if observed.get("divergence_count") != "0" or observed.get("replay") != "PASSOU":
        raise SystemExit("K2-C checker: demo reported replay divergence")
    print("K2-C command replay validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
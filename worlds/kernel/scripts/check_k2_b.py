#!/usr/bin/env python3
"""Static and artifact checks for K2-B V1 snapshot restore and persistence."""
from __future__ import annotations

import argparse
from pathlib import Path


def parse_summary(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines() if "=" in line)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    public_header = root / "include" / "minisnn_worlds_kernel_snapshot.h"
    restore = root / "src" / "minisnn_worlds_kernel_restore.c"
    app_file = root / "app" / "k2_snapshot_file.c"
    golden = root / "tests" / "golden" / "k2_a_snapshot_v1.txt"
    restore_golden = Path(args.golden)
    for path in (public_header, restore, app_file, golden, restore_golden):
        if not path.is_file():
            raise SystemExit(f"K2-B checker missing required file: {path}")
    header = public_header.read_text(encoding="utf-8")
    source = restore.read_text(encoding="utf-8")
    app_source = app_file.read_text(encoding="utf-8")
    required = (
        "minisnn_worlds_kernel_snapshot_from_bytes",
        "minisnn_worlds_kernel_create_from_snapshot",
        "MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_PRNG_VERSION",
        "MINISNN_WORLDS_KERNEL_SNAPSHOT_V1_SCALAR_SCALE",
    )
    if any(token not in header for token in required):
        raise SystemExit("K2-B checker: public V1 import/restore contract missing")
    if any(token not in source for token in (
        "restore_from_v1_bytes",
        "state_hash_versioned",
        "internal_validate_invariants",
        "_Static_assert",
        "stream_draw_count",
    )):
        raise SystemExit("K2-B checker: strict transactional restore contract missing")
    if any(token in source for token in ("fopen(", "fwrite(", "fread(", "CreateFile", "MoveFile")):
        raise SystemExit("K2-B checker: filesystem leaked into Kernel library")
    if not all(token in app_source for token in (
        "fopen(", "fread(", "fwrite(", "MoveFileExA", "snapshot_from_bytes"
    )):
        raise SystemExit("K2-B checker: app file persistence contract missing")
    frozen = dict(line.split("=", 1) for line in golden.read_text(encoding="utf-8").splitlines() if "=" in line)
    if frozen != {
        "snapshot_format_version": "1",
        "snapshot_size": "1443",
        "snapshot_digest": "0xC12509D783A5C6C0",
        "state_hash": "0xAA73D6A793A66B5B",
    }:
        raise SystemExit("K2-B checker: K2-A V1 golden changed")
    directory = Path(args.directory)
    snapshot = directory / "snapshot.bin"
    summary = directory / "summary.txt"
    if not snapshot.is_file() or not summary.is_file():
        raise SystemExit("K2-B checker: demo artifacts missing")
    data = snapshot.read_bytes()
    if len(data) < 40 or data[:8] != b"MSWKSNP1" or int.from_bytes(data[8:12], "little") != 1:
        raise SystemExit("K2-B checker: snapshot.bin is not a V1 blob")
    if int.from_bytes(data[24:32], "little") != len(data) - 40:
        raise SystemExit("K2-B checker: snapshot.bin payload length mismatch")
    summary_values = parse_summary(summary)
    if summary_values.get("snapshot_format") != "1" or summary_values.get("round_trip") != "PASSOU":
        raise SystemExit("K2-B checker: demo summary is incomplete")
    if summary_values.get("checkpoint_state_hash") != summary_values.get("restored_checkpoint_state_hash"):
        raise SystemExit("K2-B checker: checkpoint restore hash diverges")
    if summary_values.get("final_continuous_state_hash") != summary_values.get("final_restored_state_hash"):
        raise SystemExit("K2-B checker: continuation hash diverges")
    if summary_values.get("divergence_tick") != "NA":
        raise SystemExit("K2-B checker: demo reports a continuation divergence")
    expected = dict(line.split("=", 1) for line in restore_golden.read_text(encoding="utf-8").splitlines() if "=" in line)
    observed = {
        "checkpoint_tick": summary_values.get("checkpoint_tick"),
        "final_tick": summary_values.get("final_tick"),
        "snapshot_size": summary_values.get("snapshot_size"),
        "snapshot_digest": summary_values.get("snapshot_digest"),
        "checkpoint_state_hash": summary_values.get("checkpoint_state_hash"),
        "final_state_hash": summary_values.get("final_continuous_state_hash"),
    }
    if observed != expected:
        raise SystemExit("K2-B checker: continuation golden changed")
    print("K2-B restore and persistence validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3
"""Static and artifact checks for the K2-D persistence/replay closure."""
from __future__ import annotations

import argparse
from pathlib import Path

COMMAND_LOG_HEADER_SIZE = 40
COMMAND_LOG_RECORD_SIZE = 136


def parse_key_values(path: Path) -> dict[str, str]:
    return dict(
        line.split("=", 1)
        for line in path.read_text(encoding="utf-8").splitlines()
        if "=" in line
    )


def require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise SystemExit(f"K2-D checker missing {label}: {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--golden", required=True)
    args = parser.parse_args()
    root = Path(args.root).resolve()
    header = root / "include" / "minisnn_worlds_kernel_command_log.h"
    source = root / "src" / "minisnn_worlds_kernel_command_log.c"
    snapshot_header = root / "include" / "minisnn_worlds_kernel_snapshot.h"
    contract = root / "docs" / "K2_D_PERSISTENCE_CLOSURE_AUDIT.md"
    golden = Path(args.golden)
    for path, label in (
        (header, "public replay header"),
        (source, "replay implementation"),
        (snapshot_header, "snapshot header"),
        (contract, "K2-D audit"),
        (golden, "K2-D golden"),
    ):
        require_file(path, label)

    header_text = header.read_text(encoding="utf-8")
    source_text = source.read_text(encoding="utf-8")
    for token in (
        "MiniSNNWorldsKernelReplaySession",
        "minisnn_worlds_kernel_replay_session_create",
        "minisnn_worlds_kernel_replay_session_validate",
        "minisnn_worlds_kernel_replay_session_replay_next",
        "minisnn_worlds_kernel_replay_session_cursor",
    ):
        if token not in header_text:
            raise SystemExit("K2-D checker: state-bound replay API is incomplete")
    for token in (
        "expected_state_hash",
        "minisnn_worlds_kernel_state_hash",
        "REPLAY_DIVERGENCE",
        "binding_validated",
    ):
        if token not in source_text:
            raise SystemExit("K2-D checker: replay state binding is incomplete")
    if any(token in source_text for token in ("fopen(", "fwrite(", "fread(", "remove(", "rename(")):
        raise SystemExit("K2-D checker: filesystem leaked into Kernel replay code")

    directory = Path(args.directory)
    artifacts = {
        "command log": directory / "command_log.bin",
        "checkpoint snapshot": directory / "checkpoint_snapshot.bin",
        "final snapshot": directory / "final_snapshot.bin",
        "continuous final": directory / "final_continuous.bin",
        "full replay final": directory / "final_full_replay.bin",
        "restored replay final": directory / "final_restored_replay.bin",
        "summary": directory / "summary.txt",
        "hashes": directory / "hashes.txt",
    }
    for label, path in artifacts.items():
        require_file(path, label)
    log_data = artifacts["command log"].read_bytes()
    if (len(log_data) < COMMAND_LOG_HEADER_SIZE or log_data[:8] != b"MSWKLOG1" or
            int.from_bytes(log_data[8:12], "little") != 1 or
            int.from_bytes(log_data[12:16], "little") != 0):
        raise SystemExit("K2-D checker: Command Log V1 header changed")
    count = int.from_bytes(log_data[16:24], "little")
    payload_size = int.from_bytes(log_data[24:32], "little")
    if (payload_size != count * COMMAND_LOG_RECORD_SIZE or
            len(log_data) != COMMAND_LOG_HEADER_SIZE + payload_size):
        raise SystemExit("K2-D checker: Command Log V1 payload layout changed")
    checkpoint_data = artifacts["checkpoint snapshot"].read_bytes()
    if len(checkpoint_data) < 40 or checkpoint_data[:8] != b"MSWKSNP1":
        raise SystemExit("K2-D checker: Snapshot V1 checkpoint is invalid")
    final_bytes = artifacts["continuous final"].read_bytes()
    if (final_bytes != artifacts["full replay final"].read_bytes() or
            final_bytes != artifacts["restored replay final"].read_bytes() or
            final_bytes != artifacts["final snapshot"].read_bytes()):
        raise SystemExit("K2-D checker: final snapshots are not byte-identical")

    observed = parse_key_values(artifacts["summary"])
    expected = parse_key_values(golden)
    if observed != expected:
        raise SystemExit(f"K2-D checker: golden mismatch: expected {expected}, got {observed}")
    final_hashes = (
        observed.get("final_continuous_hash"),
        observed.get("final_full_replay_hash"),
        observed.get("final_restored_replay_hash"),
    )
    if len(set(final_hashes)) != 1 or None in final_hashes:
        raise SystemExit("K2-D checker: final state hashes diverge")
    if (observed.get("command_log_format_version") != "1" or
            observed.get("divergence_count") != "0" or
            observed.get("replay") != "PASSOU"):
        raise SystemExit("K2-D checker: final summary reports an invalid replay")
    hashes = parse_key_values(artifacts["hashes"])
    if set(hashes) != {"initial", "checkpoint", "continuous", "full_replay", "restored_replay"}:
        raise SystemExit("K2-D checker: hashes.txt is incomplete")
    print("K2-D state-binding and persistence validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
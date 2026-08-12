from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
from typing import Callable


Mutation = Callable[[Path], None]


def fail(message: str) -> int:
    print(f"K0-D artifact corruption validation FAILED: {message}")
    return 1


def validator_rejects(validator: Path, directory: Path) -> bool:
    result = subprocess.run([str(validator), str(directory)], text=True, capture_output=True, check=False)
    return result.returncode != 0


def clone(base: Path, target: Path) -> None:
    shutil.copytree(base, target)


def replace_line(content: str, key: str, replacement: str) -> str:
    prefix = f"{key}="
    lines = content.splitlines(keepends=True)
    matches = [index for index, line in enumerate(lines) if line.startswith(prefix)]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {key} line")
    index = matches[0]
    old_value = lines[index][len(prefix):].rstrip("\n")
    if old_value == replacement:
        raise ValueError(f"replacement did not alter {key}")
    lines[index] = f"{prefix}{replacement}\n"
    return "".join(lines)


def mutate_text(path: Path, transform: Callable[[str], str]) -> None:
    before = path.read_bytes()
    after = transform(before.decode("ascii")).encode("ascii")
    if after == before:
        raise ValueError(f"mutation did not change bytes: {path.name}")
    path.write_bytes(after)


def mutate_decimal(path: Path, key: str) -> None:
    def transform(content: str) -> str:
        prefix = f"{key}="
        value = next(line[len(prefix):] for line in content.splitlines() if line.startswith(prefix))
        if not value.isdecimal():
            raise ValueError(f"{key} is not decimal")
        return replace_line(content, key, str(int(value) + 1))

    mutate_text(path, transform)


def mutate_signature(path: Path, key: str) -> None:
    def transform(content: str) -> str:
        prefix = f"{key}="
        original = next(line[len(prefix):] for line in content.splitlines() if line.startswith(prefix))
        if not original.isdecimal():
            raise ValueError(f"{key} is not decimal")
        replacement = "1" if original == "0" else "0"
        if replacement == original:
            raise ValueError(f"signature mutation did not alter {key}")
        return replace_line(content, key, replacement)

    mutate_text(path, transform)


def mutate_hash(path: Path, key: str) -> None:
    def transform(content: str) -> str:
        prefix = f"{key}="
        original = next(line[len(prefix):] for line in content.splitlines() if line.startswith(prefix))
        if len(original) != 18 or not original.startswith("0x"):
            raise ValueError(f"{key} is not a canonical hash")
        replacement = original[:-1] + ("0" if original[-1] != "0" else "1")
        return replace_line(content, key, replacement)

    mutate_text(path, transform)


def append_text(path: Path, suffix: str) -> None:
    before = path.read_bytes()
    after = before + suffix.encode("ascii")
    if after == before:
        raise ValueError(f"append did not change bytes: {path.name}")
    path.write_bytes(after)


def remove_line(path: Path, key: str) -> None:
    def transform(content: str) -> str:
        prefix = f"{key}="
        lines = content.splitlines(keepends=True)
        kept = [line for line in lines if not line.startswith(prefix)]
        if len(kept) != len(lines) - 1:
            raise ValueError(f"expected exactly one removable {key} line")
        return "".join(kept)

    mutate_text(path, transform)


def mutate_event_field(path: Path, field_index: int, replacement: str) -> None:
    def transform(content: str) -> str:
        lines = content.splitlines(keepends=True)
        if len(lines) < 3:
            raise ValueError("integrated scenario did not produce enough events")
        fields = lines[2].rstrip("\n").split(",")
        if len(fields) != 7 or fields[field_index] == replacement:
            raise ValueError("event mutation preparation failed")
        fields[field_index] = replacement
        lines[2] = ",".join(fields) + "\n"
        return "".join(lines)

    mutate_text(path, transform)


def mutate_trace_hash(path: Path) -> None:
    def transform(content: str) -> str:
        lines = content.splitlines(keepends=True)
        if len(lines) < 2:
            raise ValueError("trace does not contain an initial row")
        fields = lines[1].rstrip("\n").split(",")
        if len(fields) != 15 or len(fields[1]) != 18 or not fields[1].startswith("0x"):
            raise ValueError("trace hash mutation preparation failed")
        fields[1] = fields[1][:-1] + ("0" if fields[1][-1] != "0" else "1")
        lines[1] = ",".join(fields) + "\n"
        return "".join(lines)

    mutate_text(path, transform)


def main() -> int:
    parser = argparse.ArgumentParser(description="Adultera copias de artefatos K0-D.")
    parser.add_argument("--runner", required=True)
    parser.add_argument("--validator", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    runner = Path(args.runner).resolve()
    validator = Path(args.validator).resolve()
    config = Path(args.config).resolve()
    root = Path(args.output_dir).resolve()
    good = root / "good"
    shutil.rmtree(root, ignore_errors=True)
    root.mkdir(parents=True)
    try:
        result = subprocess.run([str(runner), "--config", str(config), "--output", str(good)], text=True, capture_output=True, check=False)
        if result.returncode != 0 or not validator_rejects(validator, root / "missing"):
            return fail("baseline runner or missing directory contract failed")
        if subprocess.run([str(validator), str(good)], text=True, capture_output=True, check=False).returncode != 0:
            return fail("valid artifact set was rejected")

        cases: list[tuple[str, Mutation]] = [
            ("trace_truncated", lambda directory: (directory / "trace.csv").write_bytes((directory / "trace.csv").read_bytes()[:-5])),
            ("trace_hash", lambda directory: mutate_trace_hash(directory / "trace.csv")),
            ("trace_header", lambda directory: mutate_text(directory / "trace.csv", lambda text: text.replace("tick,", "missing_tick,", 1))),
            ("trace_trailing", lambda directory: append_text(directory / "trace.csv", "trailing garbage\n")),
            ("events_header", lambda directory: mutate_text(directory / "events.csv", lambda text: text.replace("event_id", "missing_id", 1))),
            ("events_trailing", lambda directory: append_text(directory / "events.csv", "trailing garbage\n")),
            ("events_regressive_id", lambda directory: mutate_event_field(directory / "events.csv", 0, "1")),
            ("events_regressive_tick", lambda directory: mutate_event_field(directory / "events.csv", 1, "0")),
            ("manifest_signature", lambda directory: mutate_signature(directory / "manifest.ini", "trace_file_signature")),
            ("manifest_count", lambda directory: mutate_decimal(directory / "manifest.ini", "trace_rows")),
            ("manifest_hash", lambda directory: mutate_hash(directory / "manifest.ini", "final_state_hash")),
            ("manifest_duplicate", lambda directory: append_text(directory / "manifest.ini", "status=OK\n")),
            ("manifest_unknown", lambda directory: append_text(directory / "manifest.ini", "unknown_key=1\n")),
            ("manifest_missing", lambda directory: remove_line(directory / "manifest.ini", "event_rows")),
            ("manifest_status", lambda directory: mutate_text(directory / "manifest.ini", lambda text: replace_line(text, "status", "BAD"))),
            ("manifest_extra", lambda directory: append_text(directory / "manifest.ini", "extra=1\n")),
            ("manifest_blank", lambda directory: append_text(directory / "manifest.ini", "\n")),
            ("manifest_missing_equals", lambda directory: append_text(directory / "manifest.ini", "missing_equals\n")),
            ("manifest_trailing_value", lambda directory: mutate_text(directory / "manifest.ini", lambda text: replace_line(text, "master_seed", "1x"))),
            ("manifest_hash_format", lambda directory: mutate_text(directory / "manifest.ini", lambda text: replace_line(text, "initial_state_hash", "0x000000000000000g"))),
            ("manifest_no_final_newline", lambda directory: mutate_text(directory / "manifest.ini", lambda text: text.rstrip("\n"))),
            ("report_empty", lambda directory: (directory / "report.txt").write_bytes(b"")),
            ("report_invalid", lambda directory: (directory / "report.txt").write_bytes(b"invalid report\n")),
            ("report_truncated", lambda directory: (directory / "report.txt").write_bytes((directory / "report.txt").read_bytes()[:-8])),
            ("report_title", lambda directory: mutate_text(directory / "report.txt", lambda text: text.replace("miniSNN", "broken", 1))),
            ("report_count", lambda directory: mutate_decimal(directory / "report.txt", "ticks")),
            ("report_hash", lambda directory: mutate_hash(directory / "report.txt", "final_state_hash")),
            ("report_signature", lambda directory: mutate_signature(directory / "report.txt", "trace_file_signature")),
            ("report_status", lambda directory: mutate_text(directory / "report.txt", lambda text: replace_line(text, "status", "BAD"))),
            ("report_duplicate", lambda directory: append_text(directory / "report.txt", "status=OK\n")),
            ("report_unknown", lambda directory: append_text(directory / "report.txt", "unknown_key=1\n")),
            ("report_workload_version", lambda directory: mutate_text(directory / "report.txt", lambda text: replace_line(text, "workload_version", "2"))),
            ("report_workload_note", lambda directory: mutate_text(directory / "report.txt", lambda text: replace_line(text, "workload_note", "changed"))),
            ("report_blank", lambda directory: append_text(directory / "report.txt", "\n")),
            ("report_missing_equals", lambda directory: append_text(directory / "report.txt", "missing_equals\n")),
            ("report_no_final_newline", lambda directory: mutate_text(directory / "report.txt", lambda text: text.rstrip("\n"))),
        ]
        for name, mutate in cases:
            candidate = root / name
            clone(good, candidate)
            try:
                mutate(candidate)
            except (OSError, ValueError, StopIteration) as error:
                return fail(f"corruption preparation failed for {name}: {error}")
            if not validator_rejects(validator, candidate):
                return fail(f"validator accepted corrupted artifact: {name}")
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("K0-D artifact corruption validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

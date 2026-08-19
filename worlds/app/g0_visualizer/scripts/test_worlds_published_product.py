#!/usr/bin/env python3
"""Verify that the published Worlds product matches the tested internal product."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile


EXPECTED_RUNTIME_LINES = {
    "world_config_format=V2",
    "water_opacity_setting=yes",
    "presentation_mode=yes",
    "transactional_resize=yes",
}
EXPECTED_STARTUP_LINES = {
    "assets_found=yes",
    "assets_initialized=yes",
    "sandbox_initialized=yes",
    "startup=PASS",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_equal(label: str, internal: Path, published: Path) -> None:
    if not internal.is_file() or not published.is_file():
        raise SystemExit(f"Published product FAIL: missing {label} executable")
    if internal.stat().st_size != published.stat().st_size:
        raise SystemExit(f"Published product FAIL: {label} size mismatch")
    if digest(internal) != digest(published):
        raise SystemExit(f"Published product FAIL: {label} hash mismatch")


def run_smoke(executable: Path, argument: str, working_directory: Path) -> set[str]:
    result = subprocess.run(
        [str(executable), argument],
        cwd=working_directory,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise SystemExit(
            f"Published product FAIL: Worlds {argument} from alternate CWD\n"
            + result.stdout + result.stderr
        )
    return set(result.stdout.splitlines())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository-root", required=True)
    parser.add_argument("--internal-worlds", required=True)
    parser.add_argument("--published-worlds", required=True)
    parser.add_argument("--internal-studio", required=True)
    parser.add_argument("--published-studio", required=True)
    args = parser.parse_args()

    repository = Path(args.repository_root).resolve()
    internal_worlds = Path(args.internal_worlds).resolve()
    published_worlds = Path(args.published_worlds).resolve()
    verify_equal("Worlds", internal_worlds, published_worlds)
    verify_equal("Studio", Path(args.internal_studio).resolve(), Path(args.published_studio).resolve())

    with tempfile.TemporaryDirectory(prefix="minisnn_worlds_startup_") as temporary:
        alternate_cwd = Path(temporary)
        startup = run_smoke(published_worlds, "--startup-smoke", alternate_cwd)
        if not EXPECTED_STARTUP_LINES.issubset(startup):
            missing = ", ".join(sorted(EXPECTED_STARTUP_LINES - startup))
            raise SystemExit("Published product FAIL: startup features missing: " + missing)
        roots = [line for line in startup if line.startswith("executable_root=")]
        if len(roots) != 1:
            raise SystemExit("Published product FAIL: startup did not report one executable root")
        reported_root = Path(roots[0].split("=", 1)[1]).resolve()
        if reported_root != repository:
            raise SystemExit(
                "Published product FAIL: executable root differs from repository assets root: "
                + str(reported_root)
            )
        runtime = run_smoke(published_worlds, "--runtime-smoke", alternate_cwd)
        if not EXPECTED_RUNTIME_LINES.issubset(runtime):
            missing = ", ".join(sorted(EXPECTED_RUNTIME_LINES - runtime))
            raise SystemExit("Published product FAIL: runtime features missing: " + missing)

    print(f"Internal Worlds: {internal_worlds}")
    print(f"Published Worlds: {published_worlds}")
    print(f"Worlds size: {internal_worlds.stat().st_size}")
    print(f"Worlds SHA256: {digest(internal_worlds)}")
    print("published_equals_internal: YES")
    print("Published Worlds product validation PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
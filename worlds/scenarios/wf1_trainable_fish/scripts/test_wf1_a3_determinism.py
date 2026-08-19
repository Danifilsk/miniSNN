from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
from pathlib import Path


def run(demo: Path, root: Path, name: str) -> Path:
    path = root / name
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    result = subprocess.run([str(demo), name], cwd=root, capture_output=True, text=True)
    if result.returncode:
        raise SystemExit(result.stdout + result.stderr)
    return path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    demo = Path(parser.parse_args().demo).resolve()
    root = demo.parents[5]
    left = run(demo, root, "build/worlds/scenarios/wf1_trainable_fish/a3_det_a")
    right = run(demo, root, "build/worlds/scenarios/wf1_trainable_fish/a3_det_b")
    try:
        for name in ("reward_probe.csv", "summary.txt"):
            if digest(left / name) != digest(right / name):
                raise SystemExit(f"WF1-A.3 determinism mismatch: {name}")
    finally:
        shutil.rmtree(left)
        shutil.rmtree(right)
    print("WF1-A.3 reward probe determinism OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
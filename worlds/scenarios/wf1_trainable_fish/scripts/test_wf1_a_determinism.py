from __future__ import annotations

import argparse
import filecmp
import shutil
import subprocess
from pathlib import Path


ARTIFACTS = ("summary.txt", "training.csv", "evaluation.csv", "seeds.csv")


def run_once(demo: Path, root: Path, relative: str) -> Path:
    output = root / relative
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    completed = subprocess.run([str(demo), relative], cwd=root, text=True,
                               capture_output=True, check=False)
    if completed.returncode != 0:
        raise SystemExit(f"WF1-A determinism: runner failed\n{completed.stdout}\n{completed.stderr}")
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", required=True)
    args = parser.parse_args()
    demo = Path(args.demo).resolve()
    root = demo.parents[5]
    first = run_once(demo, root, "build/worlds/scenarios/wf1_trainable_fish/determinism_a")
    second = run_once(demo, root, "build/worlds/scenarios/wf1_trainable_fish/determinism_b")
    for artifact in ARTIFACTS:
        if not filecmp.cmp(first / artifact, second / artifact, shallow=False):
            raise SystemExit(f"WF1-A determinism: mismatch in {artifact}")
    shutil.rmtree(first)
    shutil.rmtree(second)
    print("WF1-A determinism OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
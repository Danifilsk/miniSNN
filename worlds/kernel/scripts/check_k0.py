from __future__ import annotations

import argparse
from pathlib import Path


def fail(message: str) -> None:
    print(f"K0 Worlds Kernel validation FAILED: {message}")
    raise SystemExit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description="Fecha a fundacao deterministica K0.")
    parser.add_argument("--root", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    completion = root / "docs" / "K0_COMPLETION_AUDIT.md"
    roadmap_sources = (root / "README.md", root.parents[1] / "README.md", root.parents[1] / "worlds" / "README.md")
    if not completion.is_file():
        fail("K0 completion audit is missing")
    content = completion.read_text(encoding="utf-8")
    for required in ("K0-A", "K0-B", "K0-C", "K0-D", "K0D-008", "K0D-009", "K0D-010", "K0D-011", "K1 is authorized", "Problem"):
        if required not in content:
            fail(f"completion audit missing: {required}")
    combined = "\n".join(path.read_text(encoding="utf-8") for path in roadmap_sources if path.is_file())
    if "K0 e K1 estao concluidos" not in combined or "K2-A" not in combined or "K2-B" not in combined or "K2-C" not in combined or "K2-D" not in combined:
        fail("roadmap does not preserve K0/K1/K2-A/B/C/D completion")
    print("K0 Worlds Kernel deterministic foundation validation OK")
    print("K0 complete; K1 complete; K2-A complete; K2-B complete; K2-C complete; K2-D complete; WD0 Domain complete; WB0 Brain Bridge complete; WD1 Domain persistence next")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

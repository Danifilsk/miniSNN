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
    if "K0 esta concluido" not in combined or "K1" not in combined:
        fail("roadmap does not mark K0 complete and K1 next")
    print("K0 Worlds Kernel deterministic foundation validation OK")
    print("K0 complete; K1 authorized")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

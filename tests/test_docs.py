from pathlib import Path
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from check_docs import validate_docs


def main() -> int:
    errors = validate_docs(PROJECT_ROOT)
    if errors:
        print("Documentation validation FAILED")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Documentation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

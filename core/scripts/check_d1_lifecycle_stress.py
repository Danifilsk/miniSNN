from pathlib import Path

from d1_b_artifact_common import execute_test_and_write_artifact


if __name__ == "__main__":
    raise SystemExit(execute_test_and_write_artifact(
        "d1_b_stress.csv",
        "D1-B lifecycle, allocation and counter stress OK",
        "lifecycle stress",
        Path(__file__).resolve().parents[2],
    ))

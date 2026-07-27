from d1_b_artifact_common import execute_test_and_write_artifact


if __name__ == "__main__":
    raise SystemExit(execute_test_and_write_artifact(
        "d1_b_corruption.csv",
        "D1-B deterministic corruption and parser validation OK",
        "corruption",
    ))

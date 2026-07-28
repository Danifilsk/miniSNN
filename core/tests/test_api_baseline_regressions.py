from __future__ import annotations

from pathlib import Path
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import check_api_baseline as baseline  # noqa: E402


HEADER = """\
#ifndef SAMPLE_PUBLIC_API_H
#define SAMPLE_PUBLIC_API_H

#include <stdint.h>
/* Formatting and comments are not API. */
#define MINISNN_SAMPLE_LIMIT 42

typedef enum
{
    MINISNN_SAMPLE_A = 0,
    MINISNN_SAMPLE_B = 1
} MiniSNNExample;

typedef struct
{
    int value;
    const double *buffer;
} MiniSNNSample;

int minisnn_example(int value, const double *buffer);

#ifdef MINISNN_TESTING
int minisnn_test_only_hook(int value);
#endif

#endif
"""


def surface(text: str) -> str:
    return "\n".join(baseline.semantic_surface_from_text(text, "sample.h"))


def assert_changed(original: str, changed: str, description: str) -> None:
    if surface(original) == surface(changed):
        raise AssertionError(f"baseline did not detect {description}")


def main() -> int:
    original = surface(HEADER)
    formatted = """\
#ifndef RENAMED_GUARD
#define RENAMED_GUARD
#include <stddef.h>
#define MINISNN_SAMPLE_LIMIT 42 // contractual macro
typedef enum { MINISNN_SAMPLE_A = 0, MINISNN_SAMPLE_B = 1 } MiniSNNExample;
typedef struct { int value; const double * buffer; } MiniSNNSample;
int minisnn_example(
    int value,
    const double *buffer
);
#ifdef MINISNN_TESTING
int minisnn_test_only_hook(int value);
#endif
#endif
"""
    if original != surface(formatted.replace("\n", "\r\n")):
        raise AssertionError("formatting, comments, includes, guard, or CRLF changed the baseline")
    if "minisnn_test_only_hook" in original or "MINISNN_TESTING" in original:
        raise AssertionError("MINISNN_TESTING declaration leaked into the baseline")
    required = "function=int minisnn_example ( int value , const double * buffer ) ;"
    if required not in original:
        raise AssertionError("complete multiline prototype is missing from the baseline")

    assert_changed(HEADER, HEADER.replace("int value", "long value"), "a parameter type change")
    assert_changed(HEADER, HEADER.replace("const double *buffer", "double *buffer"), "const removal")
    assert_changed(HEADER, HEADER.replace("    int value;", "    int value;\n    int extra;"), "a public struct field")
    assert_changed(HEADER, HEADER.replace("MINISNN_SAMPLE_B = 1", "MINISNN_SAMPLE_B = 2"), "an enum value")
    before_guard, guard = HEADER.rsplit("#endif", 1)
    assert_changed(HEADER, before_guard + "int minisnn_added(void);\n\n#endif" + guard, "a public function")

    with tempfile.TemporaryDirectory(prefix="minisnn_api_baseline_") as temporary_name:
        include = Path(temporary_name)
        first = include / "first.h"
        second = include / "second.h"
        first.write_bytes(HEADER.encode("utf-8"))
        second.write_bytes(formatted.replace("\n", "\r\n").encode("utf-8"))
        first_surface = baseline.semantic_surface_from_headers([first])
        second_surface = baseline.semantic_surface_from_headers([second])
        if [item.replace("first.h", "sample.h") for item in first_surface] != [item.replace("second.h", "sample.h") for item in second_surface]:
            raise AssertionError("temporary headers did not preserve semantic equivalence")

    print("Public API baseline semantic regression validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

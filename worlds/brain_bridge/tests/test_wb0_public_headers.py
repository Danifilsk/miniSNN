#!/usr/bin/env python3
"""Compile the WB0 public header as a standalone C11 translation unit."""
from __future__ import annotations

import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 8:
        raise SystemExit("usage: test_wb0_public_headers.py CC CFLAGS bridge core domain kernel output")
    compiler, flags, bridge_include, core_include, domain_include, kernel_include, output = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="wb0_headers_") as directory:
        root = Path(directory)
        source = root / "header.c"
        object_file = root / "header.o"
        source.write_text(
            '#include "minisnn_worlds_brain_bridge.h"\nint main(void) { return 0; }\n',
            encoding="ascii",
        )
        result = subprocess.run(
            [
                compiler, *shlex.split(flags), f"-I{bridge_include}", f"-I{core_include}",
                f"-I{domain_include}", f"-I{kernel_include}", "-c", str(source),
                "-o", str(object_file),
            ],
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            raise SystemExit("WB0 public header validation FAILED:\n" + result.stdout + result.stderr)
    print("WB0 public header standalone validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
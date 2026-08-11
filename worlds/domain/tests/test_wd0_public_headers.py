#!/usr/bin/env python3
"""Validate the WD0 public header without Domain private sources."""
from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 6:
        raise SystemExit("usage: test_wd0_public_headers.py <cc> <cflags> <domain-include> <kernel-include> <out>")
    compiler, flags, domain_include, kernel_include, output = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="wd0_headers_") as directory:
        source = Path(directory) / "headers.c"
        source.write_text('#include "minisnn_worlds_domain.h"\nint main(void) { return 0; }\n', encoding="ascii")
        command = [compiler, *flags.split(), f"-I{domain_include}", f"-I{kernel_include}", str(source), "-fsyntax-only"]
        if subprocess.run(command, check=False).returncode != 0:
            raise SystemExit("WD0 public header standalone compilation failed")
    print("WD0 public header standalone validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
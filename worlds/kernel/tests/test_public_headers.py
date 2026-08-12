from __future__ import annotations

from pathlib import Path
import shlex
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 5:
        print("Worlds Kernel public header validation FAILED: argumentos invalidos")
        return 1

    compiler, cflags_text, include_directory_text, output_directory_text = sys.argv[1:]
    include_directory = Path(include_directory_text)
    output_directory = Path(output_directory_text)
    output_directory.mkdir(parents=True, exist_ok=True)
    headers = sorted(include_directory.glob("*.h"))
    if not headers:
        print("Worlds Kernel public header validation FAILED: headers ausentes")
        return 1

    for header in headers:
        source = output_directory / f"header_{header.stem}.c"
        object_file = output_directory / f"header_{header.stem}.o"
        source.write_text(f'#include "{header.name}"\nint main(void) {{ return 0; }}\n', encoding="ascii")
        command = [compiler, *shlex.split(cflags_text), f"-I{include_directory}", "-c", str(source), "-o", str(object_file)]
        completed = subprocess.run(command, text=True, capture_output=True, check=False)
        if completed.returncode != 0:
            print(f"Worlds Kernel public header validation FAILED: {header.name}")
            print(completed.stdout, end="")
            print(completed.stderr, end="")
            return 1

    print("Worlds Kernel public header validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

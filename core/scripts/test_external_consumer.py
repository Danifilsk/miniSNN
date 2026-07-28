from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from d1_c_release_common import version_string


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
RELEASE = REPOSITORY / "build" / "release" / "core"


def main() -> int:
    compiler = os.environ.get("CC", "gcc")
    with tempfile.TemporaryDirectory(prefix="minisnn_external_") as temporary_name:
        temporary = Path(temporary_name)
        include = temporary / "include"
        library = temporary / "lib"
        shutil.copytree(RELEASE / "include", include)
        shutil.copytree(RELEASE / "lib", library)
        shutil.copy2(ROOT / "examples" / "external_consumer.c", temporary / "external_consumer.c")
        executable = temporary / "external_consumer.exe"
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                   "external_consumer.c", "-I", "include", "lib/libminisnn_core.a",
                   "-o", str(executable)]
        if os.name != "nt":
            command.append("-lm")
        compiled = subprocess.run(command, cwd=temporary, text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 check=False)
        if compiled.returncode != 0:
            print(f"External consumer validation FAILED: compile\n{compiled.stdout}")
            return 1
        executed = subprocess.run([str(executable)], cwd=temporary, text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 check=False)
        expected = f"External miniSNN Core consumer OK: {version_string()}"
        if executed.returncode != 0 or expected not in executed.stdout:
            print(f"External consumer validation FAILED: run\n{executed.stdout}")
            return 1
    print("External miniSNN Core consumer validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

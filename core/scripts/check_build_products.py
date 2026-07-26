from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent


def fail(message: str) -> None:
    print(f"D1-A build products validation failed: {message}")
    raise SystemExit(1)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"nao foi possivel ler {path.relative_to(REPOSITORY)}: {error}")
    return ""


def main() -> int:
    core_makefile = read(ROOT / "Makefile")
    root_makefile = read(REPOSITORY / "Makefile")
    studio = ROOT / "studio" / "minisnn_studio.c"
    products = ROOT / "docs" / "BUILD_PRODUCTS.md"
    launcher = REPOSITORY / "Abrir miniSNN Studio.cmd"

    if not studio.is_file():
        fail("entry point do Studio nao esta em core/studio")
    if (ROOT / "app" / "minisnn_studio.c").exists():
        fail("entry point legado do Studio ainda esta em core/app")
    if (ROOT / "build").exists():
        fail("diretorio legado core/build ainda contem produtos ao lado do fonte")
    if not products.is_file():
        fail("BUILD_PRODUCTS.md ausente")
    if not launcher.is_file():
        fail("launcher clicavel do Studio ausente")

    for target in ("core:", "core-lib:", "headless:", "core-test:",
                   "clean-core:", "clean-studio:", "clean-tests:",
                   "studio-build:"):
        if target not in core_makefile:
            fail(f"target interno ausente: {target}")
    for target in ("core:", "core-lib:", "core-headless:", "core-test:",
                   "core-studio:", "studio:", "studio-path:", "clean-core:", "clean-studio:",
                   "clean-tests:"):
        if target not in root_makefile:
            fail(f"target raiz ausente: {target}")

    launcher_text = read(launcher)
    required_launcher_fragments = (
        'cd /d "%~dp0"',
        'set "STUDIO_EXE=build\\studio\\bin\\minisnn_studio.exe"',
        'where mingw32-make',
        'mingw32-make core-studio',
        'if /I "%~1"=="--build-only"',
        'start "" "%CD%\\%STUDIO_EXE%"',
    )
    for fragment in required_launcher_fragments:
        if fragment not in launcher_text:
            fail(f"launcher nao implementa o contrato: {fragment}")
    if ("C:\\Users\\" in launcher_text or "C:/Users/" in launcher_text or
            ":\\" in launcher_text or ":/" in launcher_text):
        fail("launcher contem caminho absoluto de usuario")
    if "clean" in launcher_text.lower():
        fail("launcher nao deve limpar produtos de build")

    gitignore = read(REPOSITORY / ".gitignore")
    if "*.exe" not in gitignore:
        fail("executaveis de build nao estao ignorados pelo Git")

    required_variables = (
        "CORE_LIB_DIR", "CORE_OBJ_DIR", "TOOLS_BIN_DIR", "TEST_BIN_DIR",
        "STUDIO_BIN_DIR", "STUDIO_OBJ_DIR", "CORE_LIB",
    )
    for variable in required_variables:
        if f"{variable} =" not in core_makefile:
            fail(f"produto de build nao documentado no Makefile: {variable}")

    if "$(STUDIO_BIN_DIR)/minisnn_studio.exe" not in core_makefile:
        fail("executavel do Studio nao usa diretorio exclusivo")
    if "$(CORE_LIB)" not in core_makefile or "$(AR) rcs" not in core_makefile:
        fail("biblioteca estatica autoritativa ausente")
    if "studio/minisnn_studio.c" not in core_makefile:
        fail("Makefile nao compila o entry point movido")
    if "app/minisnn_studio.c" in core_makefile:
        fail("Makefile ainda referencia Studio no diretorio antigo")
    if "core: core-lib headless" not in core_makefile:
        fail("target core nao esta isolado do Studio")
    if 'call "Abrir miniSNN Studio.cmd"' not in root_makefile:
        fail("target studio da raiz nao usa o launcher clicavel")
    if "@echo build/studio/bin/minisnn_studio.exe" not in root_makefile:
        fail("target studio-path nao informa o executavel correto")
    if "$(STUDIO_BIN_DIR)/minisnn_studio.exe:" not in core_makefile or \
       "$(STUDIO_OBJ_DIR)/minisnn_studio.o" not in core_makefile or \
       "$(CORE_LIB)" not in core_makefile.split(
           "$(STUDIO_BIN_DIR)/minisnn_studio.exe:", 1)[1].split(
               "studio-build:", 1)[0]:
        fail("Studio nao linka explicitamente a biblioteca do Core")

    studio_text = read(studio)
    for forbidden in ('#include "neuron_model.h"', '#include "network.h"',
                      '#include "minisnn_internal.h"', '#include "structure.h"'):
        if forbidden in studio_text:
            fail(f"Studio inclui header privado: {forbidden}")

    for source in (ROOT / "src").glob("*.c"):
        if "windows.h" in read(source):
            fail(f"Core depende de Win32: {source.relative_to(ROOT)}")
        if "studio/" in read(source):
            fail(f"Core depende de Studio: {source.relative_to(ROOT)}")
    for source in (ROOT / "app").glob("*.c"):
        if "windows.h" in read(source):
            fail(f"app headless depende de Win32: {source.relative_to(ROOT)}")
    for source in (ROOT / "tests").glob("*.c"):
        if "windows.h" in read(source):
            fail(f"teste headless depende de Win32: {source.relative_to(ROOT)}")

    agent_io = read(ROOT / "src" / "agent_io.c")
    if "UINT64_C(14695981039346656037)" not in agent_io or \
       "UINT64_C(1099511628211)" not in agent_io:
        fail("constantes FNV-1a 64-bit nao correspondem ao padrao")

    source_directories = (ROOT / "include", ROOT / "src", ROOT / "app",
                          ROOT / "studio", ROOT / "tests", ROOT / "scripts")
    generated = {".o", ".obj", ".exe", ".a", ".lib", ".dll"}
    for directory in source_directories:
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in generated:
                fail(f"artefato de build junto ao fonte: {path.relative_to(ROOT)}")

    print("D1-A build product separation validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

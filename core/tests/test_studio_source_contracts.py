from __future__ import annotations

from pathlib import Path
import sys


SOURCE = Path(__file__).resolve().parents[1] / "studio" / "minisnn_studio.c"
LAUNCHER_SOURCE = Path(__file__).resolve().parents[1] / "studio" / "studio_product_launcher.c"


def function_body(source: str, declaration: str) -> str:
    start = source.find(declaration)
    if start < 0:
        raise AssertionError(f"funcao ausente: {declaration}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"corpo ausente: {declaration}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise AssertionError(f"corpo sem fechamento: {declaration}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def verify_visual_ownership(source: str) -> None:
    window = function_body(source, "static LRESULT CALLBACK window_proc(")
    create_start = window.find("case WM_CREATE:")
    create_end = window.find("case WM_SIZE:", create_start)
    destroy_start = window.find("case WM_DESTROY:")
    destroy_end = window.find("default:", destroy_start)
    require(create_start >= 0 and create_end >= 0 and destroy_start >= 0 and destroy_end >= 0,
            "WM_CREATE/WM_DESTROY do Studio ausentes")

    create_body = window[create_start:create_end]
    destroy_body = window[destroy_start:destroy_end]
    owned_objects = (
        ("background_brush", "CreateSolidBrush"),
        ("edit_brush", "CreateSolidBrush"),
        ("button_pen", "CreatePen"),
        ("button_focus_pen", "CreatePen"),
        ("button_primary_pen", "CreatePen"),
        ("panel_pen", "CreatePen"),
    )
    for field, factory in owned_objects:
        assignment = f"g_app.{field} = {factory}"
        require(create_body.count(assignment) == 1,
                f"{field} deve ter exatamente uma criacao em WM_CREATE")
        require(f"DeleteObject(g_app.{field})" in destroy_body,
                f"{field} deve ser destruido em WM_DESTROY")


def verify_evolution_dialog_contracts(source: str) -> None:
    configs_helper = function_body(source, "static int evolution_configs_directory(")
    restore_helper = function_body(source, "static int restore_evolution_working_directory(")
    chooser = function_body(source, "static void choose_evolution_config(void)")
    saver = function_body(source, "static int save_evolution_dialog_config(int choose_path)")

    require('project_path(out_path, out_path_size, "configs")' in configs_helper,
            "configs evolutivos devem ser resolvidos pelo runtime layout")
    require("SetCurrentDirectoryA(working_directory)" in restore_helper,
            "helper de restauracao deve usar o diretorio de runtime")
    for name, body, api in (
        ("abrir", chooser, "GetOpenFileNameA"),
        ("salvar", saver, "GetSaveFileNameA"),
    ):
        require("evolution_configs_directory(configs_path, sizeof(configs_path))" in body,
                f"dialogo de {name} deve resolver configs pelo runtime layout")
        require("ofn.lpstrInitialDir = configs_path;" in body,
                f"dialogo de {name} deve usar caminho absoluto de configs")
        require("OFN_NOCHANGEDIR" in body,
                f"dialogo de {name} deve usar OFN_NOCHANGEDIR")
        call = body.find(api)
        restore = body.find("restore_evolution_working_directory(working_directory)", call)
        require(call >= 0 and restore > call,
                f"dialogo de {name} deve restaurar o diretorio apos o dialogo")
        if name == "salvar":
            require(
                body[call:].count("restore_evolution_working_directory(working_directory)") >= 2,
                "dialogo de salvar deve restaurar o diretorio no cancelamento e no sucesso",
            )


def verify_worlds_launcher_contract(source: str, launcher: str) -> None:
    button_handler = function_body(source, "static void open_minisnn_worlds(void)")

    require('#include "studio_product_launcher.h"' in source,
            "Studio deve usar somente o helper de launcher do Worlds")
    require("IDC_BTN_WORLDS" in source and "ABRIR MINISNN WORLDS" in source,
            "Studio deve expor o botao principal do Worlds")
    require("case IDC_BTN_WORLDS:" in source and "open_minisnn_worlds();" in source,
            "botao Worlds deve acionar o launcher")
    require("ShowWindow(g_app.worlds_button, SW_SHOWNA);" in source and
            "MoveWindow(g_app.worlds_button" in source,
            "botao Worlds deve permanecer visivel no layout principal")
    require("GetModuleFileNameA" in button_handler,
            "launcher deve resolver o executavel do proprio Studio")
    require("studio_product_launcher_resolve" in button_handler and
            "studio_product_launcher_launch" in button_handler,
            "Studio deve resolver e iniciar Worlds pelo helper")
    require("minisnn_worlds_" not in source and '"wf0_' not in source and
            '"minisnn_worlds_' not in launcher,
            "Studio launcher nao pode incluir APIs internas do Worlds")
    require("minisnn_worlds.exe" in launcher and "CreateProcessA" in launcher,
            "helper deve localizar e iniciar o produto Worlds separado")

def main() -> int:
    try:
        source = SOURCE.read_text(encoding="utf-8")
        launcher = LAUNCHER_SOURCE.read_text(encoding="utf-8")
        verify_visual_ownership(source)
        verify_evolution_dialog_contracts(source)
        verify_worlds_launcher_contract(source, launcher)
    except (OSError, AssertionError) as error:
        print(f"Studio source contracts validation FAILED: {error}")
        return 1
    print("Studio source contracts validation OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

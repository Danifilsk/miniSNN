# Guia rápido

## Comece por aqui

Em cinco minutos:

1. Abra um terminal MSYS2 UCRT64 na raiz do projeto.
2. Execute `mingw32-make test`.
3. Compile o Studio com `mingw32-make studio-build`.
4. Abra `.\build\minisnn_studio.exe` e rode `configs/random.ini`.
5. Use `GERAR DIAGNOSTICO` ou rode o analisador pelo terminal.

## Requisitos

- Windows.
- MSYS2 UCRT64.
- GCC/MinGW.
- `mingw32-make`.
- Python opcional para gráficos e diagnóstico.
- pandas e matplotlib para os scripts de análise.

Não há versões mínimas oficiais. A execução de referência do A5 usou GCC 16.1,
Python 3.14, pandas 3.0 e matplotlib 3.11; outras versões compatíveis podem
funcionar, mas não estão garantidas por essa observação.

## Compilar e testar

```powershell
mingw32-make clean
mingw32-make test
mingw32-make studio-build
```

O executável do Studio é:

```text
build/minisnn_studio.exe
```

Abra com:

```powershell
.\build\minisnn_studio.exe
```

## Executar cenário pelo terminal

```powershell
mingw32-make scenario-random
mingw32-make scenario-small-world
```

Esses comandos usam `configs/random.ini` e `configs/small_world.ini`.

## Gerar diagnóstico

```powershell
python scripts/analyze_run.py results/scenarios/random_demo --level basic
```

Use `--level full` para a análise mais cara e amostrada. O nível `off` evita a
análise pesada.

## Comparar execuções

```powershell
python scripts/compare_runs.py `
  results/scenarios/random_demo `
  results/scenarios/small_world_demo `
  --out-name random_vs_small_world
```

## Resultados

```text
results/scenarios/<actual_run_name>/
results/comparisons/<comparison_name>/
```

Se o Studio criar um nome com timestamp, use a pasta mostrada como `Pasta real`.

## Próximas leituras

- [Guia do Studio](GUIA_DO_STUDIO.md)
- [Guia de cenários](GUIA_DE_CENARIOS.md)
- [Organização de resultados](ORGANIZACAO_DE_RESULTADOS.md)
- [Índice completo](INDICE_DA_DOCUMENTACAO.md)

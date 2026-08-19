# Produtos de build

## Abrir o Studio

No Windows, o caminho normal e dar dois cliques em
`Abrir miniSNN Studio.cmd` na raiz do repositorio. O launcher encontra a raiz
pelo proprio diretorio, compila somente quando
`build/studio/bin/minisnn_studio.exe` ainda nao existe e abre esse aplicativo.
Use `call "Abrir miniSNN Studio.cmd" --build-only` para apenas validar ou criar
o executavel em uma automacao.

`core/studio/minisnn_studio.c` e codigo-fonte do frontend, nao um executavel.
O produto clicavel e `build/studio/bin/minisnn_studio.exe`, ligado contra a
biblioteca neural `build/core/lib/libminisnn_core.a`.

O consumidor visual Worlds G0-B usa a mesma convencao de entrada. No Makefile
da raiz do monorepo, o alvo `worlds` gera `build/studio/bin/minisnn_studio.exe` e
`build/studio/bin/minisnn_worlds.exe` lado a lado. O Worlds e independente do
Studio e localiza seus sprites a partir do proprio executavel; o Studio apenas
resolve e inicia o executavel vizinho pelo botao `ABRIR MINISNN WORLDS`.

## O que e cada produto

`miniSNN Core` e a biblioteca neural headless. Sua API publica esta em
`include/` e sua implementacao esta em `src/`. O artefato autoritativo e
`../build/core/lib/libminisnn_core.a`.

`miniSNN Studio` e um consumidor Win32 da biblioteca, nao a biblioteca. Seu
entry point e `studio/minisnn_studio.c`; o executavel e
`../build/studio/bin/minisnn_studio.exe`.

Os runners, demos e a ferramenta de evolucao sao aplicacoes headless em
`app/`. Seus executaveis ficam em `../build/tools/bin`. Testes C ficam em
`../build/tests/bin`. Objetos do Core e do Studio ficam, respectivamente, em
`../build/core/obj` e `../build/studio/obj`.

```text
miniSNN Core library
        ^
        |-- headless runners
        |-- evolution tool
        |-- tests
        `-- miniSNN Studio
```

## Comandos

| Comando | Produto |
|---|---|
| `mingw32-make core-lib` | Somente a biblioteca estatica. |
| `mingw32-make core` | Biblioteca e apps headless padrao; nunca Studio. |
| `mingw32-make core-headless` | Runners e ferramenta de evolucao headless. |
| `mingw32-make core-test` | Suite de testes sem GUI. |
| `mingw32-make core-evolution` | Runner de evolucao em `build/tools/bin`. |
| `mingw32-make core-studio` | Compila o Studio explicitamente, sem abrir. |
| `worlds` (alvo da raiz) | Compila Studio e Worlds como executaveis vizinhos em `build/studio/bin`. |
| `mingw32-make studio` | Compila quando necessario e abre o Studio. |
| `mingw32-make studio-path` | Mostra `build/studio/bin/minisnn_studio.exe`. |
| `mingw32-make clean-core` | Biblioteca e objetos do Core. |
| `mingw32-make clean-studio` | Somente produto Studio. |
| `mingw32-make clean-tests` | Somente executaveis de testes. |
| `mingw32-make clean` | Todos os produtos recriaveis conhecidos. |

Os alvos historicos continuam como wrappers. Resultados cientificos pertencem
a `results/`, nunca a `build/`.

## Fronteiras e achados D1-A

- O Core nao inclui `windows.h` nem headers do Studio.
- O Studio inclui apenas headers publicos do Core e interfaces de aplicacao
  headless; ele nao inclui `src/`.
- `app/evolution_config.h` usa `include/minisnn_evolution_legacy.h`, uma
  superficie historica necessaria ao painel de evolucao. Seus nomes sem o
  prefixo `minisnn_` sao uma pendencia de migracao compatvel para D1-B/D1-C,
  nao uma nova API congelada.
- Testes instrumentados com `MINISNN_TESTING` sao a excecao explicita ao link
  contra a biblioteca normal; os demais testes e todos os consumidores
  produtivos usam `libminisnn_core.a`.

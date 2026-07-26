# D1-A - Auditoria de arquitetura, produtos e API candidata

## Estrutura anterior e posterior

Antes de D1-A, o entry point do Studio compartilhava a pasta de apps headless e
o alvo do Studio recompilava a selecao de fontes do Core. Executaveis eram
gerados em `core/build/`. Depois da passagem, `studio/minisnn_studio.c` e o
unico entry point Win32, `src/*.c` gera `build/core/lib/libminisnn_core.a` uma
vez, runners ficam em `build/tools/bin`, testes em `build/tests/bin` e Studio
em `build/studio/bin`.

## Acesso ao aplicativo

O aplicativo clicavel e `build/studio/bin/minisnn_studio.exe`, nao
`core/studio/minisnn_studio.c`, que permanece apenas como codigo-fonte do
frontend. Para o uso normal no Windows, de dois cliques em
`Abrir miniSNN Studio.cmd` na raiz. O launcher usa caminhos relativos, compila
o Studio com `core-studio` somente quando o executavel nao existe e o abre. A
biblioteca neural entregue separadamente permanece em
`build/core/lib/libminisnn_core.a`.

## Achados

| ID | Severidade | Area | Problema | Correcao/teste | Estado |
|---|---|---|---|---|---|
| D1A-01 | alta | build | Studio recompilava Core | Biblioteca estatica e link explicito; audit de produtos | corrigido |
| D1A-02 | media | layout | GUI dentro de `app/` | Movida para `studio/`; checks C2-C4 atualizados | corrigido |
| D1A-03 | media | portabilidade | filesystem headless usava Win32 | `app_filesystem` e Core sem `windows.h` | corrigido |
| D1A-04 | media | API | parser de modelo era interno em apps/Studio | wrappers publicos centralizados em `minisnn.h` | corrigido |
| D1A-05 | media | evolucao | tipos historicos sem namespace | manifesto `LEGACY_SUPPORTED` | pendente D1-B/C |
| D1A-06 | baixa | ABI | objetos de app ainda compilam por alvo | Core e Studio possuem objetos separados; consolidacao total de apps fica para D1-B | registrado |

## Riscos e limites

O Studio usa interfaces de configuracao de `app/` para compartilhar parser e
execucao com o terminal. Isso nao expõe internals neurais nem requer o Studio
para qualquer teste, runner ou evolucao. A revisao de uma facade publica de
evolucao namespaced, ABI e stress multi-toolchain pertence a D1-B/D1-C.

Nao ha declaracao de v1.0-rc, API congelada ou autorizacao de Worlds nesta
etapa.

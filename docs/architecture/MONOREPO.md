# Arquitetura do monorepo

## M1: miniSNN Core

`core/` contem a biblioteca, os runners, o Studio, configuracoes, testes,
scripts, exemplos, experimentos e resultados do miniSNN atual. A migracao M1
e apenas fisica: nao altera comportamento neural, equacoes, seeds, fitness ou
formatos cientificos.

## Limites planejados

- Core nao depende de Worlds.
- Worlds Kernel sera uma biblioteca generica independente e nao dependera do
  Core.
- Brain Bridge sera o unico modulo que conhecera Core e Domain.
- Worlds, Worlds Kernel, Domain e Brain Bridge nao sao implementados nesta
  migracao.

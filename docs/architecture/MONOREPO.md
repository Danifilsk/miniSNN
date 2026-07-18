# Arquitetura do monorepo

## M1: miniSNN Core

`core/` contem a biblioteca, os runners, o Studio, configuracoes, testes,
scripts, exemplos, experimentos e resultados do miniSNN atual. A migracao M1
e apenas fisica: nao altera comportamento neural, equacoes, seeds, fitness ou
formatos cientificos.

## C5: modelos neuronais avançados

O Core possui uma fronteira interna comum para redes homogêneas LIF, AdEx e
Hodgkin-Huxley. O C5 foi concluído sem introduzir redes híbridas ou
dependências para os módulos futuros do monorepo.

## C7.1: contratos cerebro-agente

O Core agora expoe schemas e frames numericos por `core/include/minisnn_agent_io.h`.
Essa interface permanece independente de qualquer dominio e nao cria uma
dependencia de Worlds. A ponte que conectara I/O numerico a SNN permanece
planejada para C7.2.

## Limites planejados

- Core nao depende de Worlds.
- Worlds Kernel sera uma biblioteca generica independente e nao dependera do
  Core.
- Brain Bridge sera o unico modulo que conhecera Core e Domain.
- Worlds, Worlds Kernel, Domain e Brain Bridge nao sao implementados nesta
  migracao.

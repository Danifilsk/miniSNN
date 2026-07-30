# Arquitetura do monorepo

## D1 concluido: release candidate local

O produto `miniSNN Core` tem versao tecnica `1.0.0-rc.1`. O Core e distribuido
separadamente do miniSNN Studio em pacotes locais de avaliacao; a Bridge entre
Core e Worlds ainda e candidata. D1 esta concluido para `miniSNN Core
1.0.0-rc.1`; K0 esta concluido no Worlds Kernel deterministico.

## K0-A a K0-D: Worlds Kernel

`worlds/kernel/` e um produto fisico C11, headless e independente para o
Worlds Kernel. K0-A conclui configuracao, lifecycle, tempo logico, diagnostico
e tick atomico. K0-B conclui Entity IDs, comandos futuros, resolucao canonica,
eventos e preflight atomico. K0-C conclui PRNG por streams, hash canonico e
observabilidade. K0-D conclui parser de cenario, runner, artefatos tecnicos,
determinismo integrado, stress e auditoria sem levar filesystem para a
biblioteca. O Kernel nao depende do Core, e o Core nao depende do Kernel.
K0 esta concluido; K1 e o proximo bloco de transforms, espaco, ocupacao,
barreiras, deslocamento e links. K2 vem depois para snapshots, save/load e
replay.

## D1-A: produtos de build e API candidata

`core/include/` e `core/src/` formam a biblioteca headless. `core/app/` contem
runners e demos headless; `core/studio/` contem somente a GUI Win32. A
biblioteca estatica fica em `build/core/lib`, ferramentas em `build/tools/bin`,
testes em `build/tests/bin` e o Studio em `build/studio/bin`. D1 nao declara
Core 1.0, API congelada definitivamente ou Worlds implementado.

## D1-B: robustez headless

Os targets D1-B compilam somente biblioteca, testes e ferramentas em
`build/core/lib/`, `build/tests/bin/` e `build/tools/bin/`. Eles nao compilam
nem abrem o Studio. Os artefatos regeneraveis ficam em
`core/results/d1_b_robustness/` e sao ignorados por Git.

## C7.5-A - Persistencia e replay do ciclo cerebro-agente

`core/src/agent_cycle_checkpoint.c` persiste, em arquivos internos
versionados, a rede, AgentIO, encoder, decoder e estado temporal do ciclo.
`core/src/agent_cycle.c` retem a maquina de estados e a API publica. O
checkpoint aceita somente fronteiras `READY` e `ACTION_PENDING`, verifica
assinaturas e hashes antes de restaurar e nunca serializa ponteiros. O demo
continua em `core/app/`; essa persistencia nao introduz semantica de Worlds.

## C7.4 - Ciclo generico cerebro-agente

`core/include/minisnn_agent_cycle.h` e `core/src/agent_cycle.c` pertencem ao
Core. O ciclo faz a orquestracao publica entre AgentIO, encoder, rede, decoder,
feedback C2 e reset transiente. Ele permanece numerico, nao proprietario e sem
semantica de dominio. O demo e parser INI ficam em `core/app/`.

## C7.3 - Decodificacao neural-acao

O modulo publico `core/include/minisnn_action_decoder.h` e sua implementacao
em `core/src/action_decoder.c` pertencem ao Core. O parser e demo
`core/app/action_decoding_demo_config.c` e `core/app/action_decoding_demo.c`
ficam na camada de aplicacao. Nenhum modulo C7 contem semantica de Worlds.

## M1: miniSNN Core

`core/` contem a biblioteca, os runners, o Studio, configuracoes, testes,
scripts, exemplos, experimentos e resultados do miniSNN atual. A migracao M1
e apenas fisica: nao altera comportamento neural, equacoes, seeds, fitness ou
formatos cientificos.

## C5: modelos neuronais avançados

O Core possui uma fronteira interna comum para redes homogêneas LIF, AdEx e
Hodgkin-Huxley. O C5 foi concluído sem introduzir redes híbridas ou
dependências para os módulos futuros do monorepo.

## C7.1-C7.5: contratos, codificacao, decodificacao, ciclo, replay e auditoria

O Core expoe schemas e frames numericos por `core/include/minisnn_agent_io.h`
e codificacao deterministica por `core/include/minisnn_sensor_encoder.h`.
Essa interface permanece independente de qualquer dominio e nao cria uma
dependencia de Worlds. C7.2 aplica correntes por API publica sem avancar a
rede; C7.3 decodifica atividade completa sem avancar a rede; C7.4 e o unico
orquestrador C7 que avanca a rede e publica a action atomicamente. C7.5-A
adiciona persistencia e replay verificados; C7.5-B fecha a auditoria por
matriz, ciclo de vida e long run. C7, D1 e K0 estao concluidos; K1 e o proximo
bloco planejado.

## Limites planejados

- Core nao depende de Worlds.
- Worlds Kernel e uma biblioteca generica independente e nao depende do Core.
- Brain Bridge sera o unico modulo que conhecera Core e Domain.
- Worlds Domain, Brain Bridge e Worlds App ainda nao sao implementados.

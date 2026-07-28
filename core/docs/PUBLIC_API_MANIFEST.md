# Manifesto da API publica candidata

Status: **CANDIDATE_V1** em D1-A. Esta lista e deterministica, documenta a
superficie publica atual e nao declara congelamento definitivo.

Em D1-C, a versao tecnica candidata e `1.0.0-rc.1`. A superficie continua
provisoria ate D2, quando a integracao com um consumidor Worlds real podera
congelar a Core-Brain Bridge v1 definitivamente.

O baseline candidato usa `baseline_format=minisnn_public_api_v2`: ele registra
declaracoes completas (funcoes, tipos, enums, campos e macros contratuais),
mas ignora comentarios, whitespace, CRLF/LF, includes, include guards e blocos
`MINISNN_TESTING`. A migracao para v2 e uma melhoria do formato de auditoria,
nao uma mudanca da API publica.

## Convencoes

- Funcoes novas de producao usam `minisnn_`; tipos novos usam `MiniSNN`; macros
  novas usam `MINISNN_`.
- Retorno `0` e falha para operacoes booleanas. `minisnn_step` retorna `-1` em
  erro neuronal. Ownership, erros, limites e persistencia sao definidos em
  [OWNERSHIP_AND_LIFETIME](OWNERSHIP_AND_LIFETIME.md),
  [ERROR_MODEL](ERROR_MODEL.md), [LIMITS](LIMITS.md) e
  [PERSISTENCE_COMPATIBILITY](PERSISTENCE_COMPATIBILITY.md).
- `PRODUCTION` identifica API da biblioteca. `TEST_ONLY` exige
  `MINISNN_TESTING`. `LEGACY_SUPPORTED` continua disponivel por compatibilidade
  e sera revisto antes de v1.0-rc.

## Rede e modelos

| Header | Tipos e funcoes | Ownership/erro | Classe | Consumidores |
|---|---|---|---|---|
| `minisnn.h` | `MiniSNN`, `MiniSNNConfig`; `minisnn_create*`, `minisnn_destroy`, `minisnn_step`, configuracao de corrente, conexao, tipos, topologia, getters, reset, signatures e checkpoint de rede | Rede e opaca; destroy por ponteiro duplo; getters copiam ou devolvem escalar | CANDIDATE_V1 / PRODUCTION | headless, Studio, evolution, Bridge futuro |
| `minisnn_types.h` | `MiniSNNNeuronModel`, LIF/AdEx/HH configs e states; conexao, plasticidade, reward, homeostase e estruturas de diagnostico | Structs por valor; parametros validados no create/set | CANDIDATE_V1 / PRODUCTION | todos |
| `minisnn_version.h` | macros e `minisnn_version_*` para `1.0.0-rc.1` | strings estaticas, sem alocacao ou estado | CANDIDATE_V1 / PRODUCTION | todos |
| `minisnn.h` | `minisnn_neuron_model_name`, `minisnn_neuron_model_from_name`, `minisnn_neuron_model_is_valid`, `minisnn_config_is_valid` | Nome e constante nao proprietaria; parser nao retencao input | CANDIDATE_V1 / PRODUCTION | runners, Studio |

## Plasticidade, estrutura e evolucao

| Header | Tipos e funcoes | Classe | Consumidores |
|---|---|---|---|
| `minisnn.h` / `minisnn_types.h` | `minisnn_default_*_config`, `minisnn_set_*_config`, stats e getters de STDP, R-STDP, homeostase e estrutura | CANDIDATE_V1 / PRODUCTION | headless, evolution |
| `minisnn_evolution_legacy.h` | `EvolutionEngine`, `EvolutionIndividual`, `EvolutionPrng`, `evolution_engine_*`, `evolution_fitness_*` | LEGACY_SUPPORTED / PRODUCTION | evolution runner, Studio via config, tests |

`minisnn_evolution_legacy.h` e a unica excecao documentada de nomes historicos
sem prefixo. Ela nao e uma recomendacao para novas APIs e e candidata a uma
camada namespaced de compatibilidade em D1-B/D1-C.

## Memoria C6

Os protocolos de memoria de trabalho, associativa e sequencial pertencem a
`app/` como laboratorio cientifico. Eles constroem `MiniSNN` somente com
interfaces publicas. Seus resultados e fitness opcionais sao
**INTERNAL/EXPERIMENTAL**, nao uma API de memoria geral.

## AgentIO e sensor encoder

| Header | Tipos e funcoes | Ownership/erro | Classe | Consumidores |
|---|---|---|---|---|
| `minisnn_agent_io.h` | schemas, `MiniSNNSensorFrame`, `MiniSNNActionFrame`, `MiniSNNAgentIOContext`; create/destroy, submit/consume, reset, signature e `last_error` | schemas copiados pelo contexto; frames copiados; consume exige buffer do chamador | CANDIDATE_V1 / PRODUCTION | C7, Studio, Bridge futuro |
| `minisnn_sensor_encoder.h` | `MiniSNNSensorEncoder`, mappings e `MiniSNNNeuralInputFrame`; create, encode, apply, save/load e signature | frame e schema possuem destroy proprio; apply e atomico e nao avanca rede | CANDIDATE_V1 / PRODUCTION | C7, Bridge futuro |

## Action decoder e Agent Cycle

| Header | Tipos e funcoes | Ownership/erro | Classe | Consumidores |
|---|---|---|---|---|
| `minisnn_action_decoder.h` | `MiniSNNActionDecoder`, specs, diagnosticos, decode, decode_to_agent_io, save/load e signature | decoder copia schema; incompatibilidade de schema falha antes de publicar action | CANDIDATE_V1 / PRODUCTION | C7, Bridge futuro |
| `minisnn_agent_cycle.h` | `MiniSNNAgentCycle`, create/destroy, tick, feedback, reset, save/load e status | ciclo nao possui a rede nem AgentIO; checkpoint e validado antes de restaurar | CANDIDATE_V1 / PRODUCTION | C7, Bridge futuro |

## Exclusoes

- `src/*_internal.h`, `network.h`, `neuron.h`, `structure.h` e helpers de
  modelo sao **INTERNAL**.
- Hooks guardados por `MINISNN_TESTING` sao **TEST_ONLY** e nao entram no
  arquivo estatico de producao.
- `studio/` e **PRESENTATION_ONLY**; nao exporta API Core.

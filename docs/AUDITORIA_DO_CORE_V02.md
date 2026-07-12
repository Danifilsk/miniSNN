# Auditoria do Core v0.2

## Escopo e estado

Esta auditoria cobre a fundação da **miniSNN Core v0.2 - Essential Lab**. Ela
verifica a implementação existente; não acrescenta plasticidade, novos modelos
neurais ou novas hipóteses científicas. O estado inicial foi o commit
`48f119e`, branch `main`, com working tree limpo.

Status automático: **PASSOU**. Validação manual do Studio: **PENDENTE**.

## Matriz da suíte

| Teste | Tipo | Componente | O que verifica concretamente | O que não verifica | Resultado |
|---|---|---|---|---|---|
| `test_minisnn_api` | numérico, estrutural, integração | API pública | configuração, entradas, conexões, delay, getters e ciclo de vida | topologias do runner e GUI | PASSOU |
| `test_topology` | numérico, estrutural, integração | núcleo interno | listas, delays, topologias internas, estímulos e recorders | caminho de topologia do Studio | PASSOU |
| `test_LIF` | numérico | LIF | Euler independente, trajetória, limiar, reset, spike exato e inibição | fidelidade biológica | PASSOU |
| `test_scenario_config` | estrutural, robustez | parser INI | defaults, round-trip, duplicatas, limites e não finitos | permissões globais | PASSOU |
| `test_scenario_runner` | integração, robustez | runner | arquivos, histórico, diagnósticos e colisões de path | aparência/cliques | PASSOU |
| `test_runner_topologies` | estrutural | runner | pares, contagens, pesos, delays, tipos e assinatura das sete topologias | propriedades estatísticas clássicas | PASSOU |
| `test_reproducibility` | regressão, integração | runner/Studio | mesma seed, estrutura, CSV e dinâmica; seeds conhecidas diferentes | compiladores diferentes | PASSOU |
| `test_memory_stress` | robustez | rede/API | criação repetida, realloc, rede densa e delays | leak detector externo | PASSOU |
| `test_long_run` | robustez | rede/API | 50.000 passos e estado finito | execução indefinida | PASSOU |
| `test_metrics_common` | numérico | métricas | burst, Gini, entropia, correlação, estabilidade e oito regimes | significado biológico | PASSOU |
| `test_analyze_run` | numérico, integração | diagnóstico | população, EXC/INH, ISI e neurônio detalhado | PNG pixel a pixel | PASSOU |
| `test_compare_runs` | numérico, integração | comparação | valores armazenados, fallback legado e outputs | equivalência científica | PASSOU |
| `test_plot_neuron` | integração, smoke | gráfico individual | leitura e geração do PNG | interpretação do gráfico | PASSOU |
| `test_script_robustness` | robustez | Python | ausência, vazio, colunas, não finitos, uma linha, raster vazio e paths com espaços | todas as exceções do pandas | PASSOU |
| `test_regression_baseline` | regressão | runner | golden pequeno e totais históricos | estabilidade entre arquiteturas | PASSOU |
| `test_docs` | estrutural | documentação | links, arquivos, alvos, chaves e índice | revisão científica humana | PASSOU |

## LIF numérico

O teste usa parâmetros simples e uma referência calculada independentemente:

```text
V(t + dt) = V(t) + (dt / tau) * (-(V(t) - V_rest) + R * I)
```

A tolerância absoluta e relativa é `1e-12`, adequada às poucas operações em
`double` dos casos escolhidos. Primeiro spike, reset e total no horizonte fixo
são verificados exatamente.

## Caminhos de topologia

| Topologia | Núcleo interno | Runner e Studio | Cobertura real |
|---|---|---|---|
| chain, ring, all-to-all, random | `src/topology.c` | `app/scenario_runner.c` | estrutural + golden |
| random balanced | `src/topology.c` | `app/scenario_runner.c` | estrutural + seed + golden |
| small-world, feedforward | ausente no módulo interno | `app/scenario_runner.c` | estrutural + seed + golden |

O Studio chama `scenario_runner`. A centralização foi adiada porque alteraria
seeds e resultados com risco maior que o benefício neste marco. A duplicação fica
como dívida técnica. O runner agora fornece assinatura determinística leve por
origem, alvo, bits do peso, delay e tipo da origem.

## Baseline e reprodutibilidade

`tests/golden/core_v02_baseline.json` guarda somente assinatura, contagem, spikes
por timestep, raster normalizado, cabeçalhos e métricas principais. Timestamps,
paths, wall time e estado Git ficam fora. Mesma seed reproduziu estrutura, CSVs e
dinâmica; seeds conhecidas diferiram; seed não afetou topologias determinísticas.

## Bugs confirmados e corrigidos

1. `burst_z_threshold = 0` era convertido em `2.0`; zero agora é preservado.
2. Histórico existente sem cabeçalho recebia linhas; agora é recusado sem ser alterado.
3. `NaN`/infinito em dados obrigatórios podia contaminar análise; agora há erro ou descarte explícito.

Nenhuma correção altera equação LIF, ordem temporal, pesos, delays ou dinâmica de
configurações válidas.

## Análise estática, memória e sanitizers

Flags aprovadas:

```text
-std=c11 -Wall -Wextra -pedantic -fanalyzer
-Wformat=2 -Wshadow -Wnull-dereference
```

O MinGW GCC 16.1 reconhece `-fsanitize=address,undefined`, mas não possui
`libasan`/`libubsan`: **NÃO SUPORTADO** neste toolchain. `test-sanitize` relata a
limitação. Não foram encontrados erros de ciclo de vida, mas isso não prova
ausência de leaks sem runtime sanitizer.

## Resultados conhecidos

| Cenário | Antes | Depois | Estado |
|---|---:|---:|---|
| `random_demo` | 6757 | 6757 | preservado |
| `small_world_demo` | 15045 | 15045 | preservado |

## Declarações científicas

**Fortemente validado na implementação atual:** LIF discreto nos casos testados,
invariantes estruturais, delays, ciclo de vida, determinismo no mesmo binário e
fórmulas principais de métricas.

**Parcialmente validado:** portabilidade, grandes escalas, falhas raras de
alocação e pipeline visual completo.

**Heurístico:** regimes, proxy de sincronia e `stability_score`.

**Não validado:** fidelidade biológica, ASan/UBSan neste Windows e equivalência
entre compiladores/arquiteturas.

**Planejado:** revisão humana do Studio e sanitizers em ambiente compatível.


# Arquitetura do Core

## Visão geral

O fluxo de um cenário é:

```text
arquivo .ini
  -> scenario_config
  -> scenario_runner
  -> API pública miniSNN
  -> network / neuron
  -> CSVs da execução
  -> scripts Python
  -> métricas, relatórios e gráficos
  -> miniSNN Studio como interface do fluxo
```

O Studio e o runner de terminal compartilham o mesmo parser e executor. A
interface não possui uma segunda implementação da dinâmica neural.

## Camadas

| Camada | Responsabilidade |
|---|---|
| `src/` | Implementação interna do LIF, rede, conexões, delays, estímulos, recorders e fachada pública. |
| `include/` | API pública opaca usada por aplicações externas. |
| `app/` | Parser INI, construção de topologias, execução de cenários e Studio Win32. |
| `configs/` | Cenários reproduzíveis sem edição de C. |
| `scripts/` | Gráficos, diagnóstico e comparação posterior à execução. |
| `tests/` | Testes C e Python com escopos diferentes. |
| `examples/` | Exemplos públicos e demo interno. |
| `experiments/` | Estudos exploratórios em C, separados do uso normal. |
| `results/` | Saídas locais, comparações e resultados científicos preservados. |
| `docs/` | Guias técnicos e operacionais. |

## Core, Studio e Worlds

**IMPLEMENTADO — miniSNN Core:** laboratório neural e motor experimental.

**IMPLEMENTADO — miniSNN Studio:** interface Windows para criar, executar,
observar e comparar cenários do Core.

**PLANEJADO — miniSNN Worlds:** futura camada de simulação de agentes e vida.
Ela não existe no código atual e não deve ser confundida com o Studio.

## Encapsulamento

`MiniSNN` é um tipo opaco declarado em `include/minisnn.h`. O usuário cria e
consulta uma rede por funções públicas, sem depender de `Network`, vetores
internos ou detalhes de alocação. O runner usa somente essa API para configurar
tipos, conectar, aplicar entrada, avançar timesteps e consultar resultados.

O núcleo interno ainda oferece módulos diretos para testes, demo e experimentos
legados. Eles não fazem parte da garantia pública de encapsulamento.

## Fluxo temporal resumido

Em cada timestep, o runner limpa entradas externas, aplica a corrente às fontes,
chama `minisnn_step()` e consulta spikes, tensão e corrente sináptica usada. A
rede mantém delays e corrente pendente internamente. Os CSVs registram o estado
observado depois do update, com a corrente sináptica efetivamente usada pelo LIF.

Com STDP ativo, `src/plasticity.c` executa depois que os spikes atuais foram
transmitidos com o peso anterior. O módulo decai traces, acumula deltas LTD pelas
listas de saída e LTP por um índice de entradas, aplica um clamp por conexão e
só então incrementa os traces. O índice guarda `(source, outgoing_index)`, não
ponteiros sujeitos a `realloc`.

## Observabilidade

O runner produz `population.csv`, `raster.csv`, um CSV do neurônio detalhado,
resumo, configuração usada e manifesto. O diagnóstico Python deriva métricas e
gráficos sem alterar a simulação concluída.

## Limitações confirmadas

- O Studio depende de Win32 e o build principal foi validado no Windows/MSYS2.
- O diagnóstico completo depende de Python, pandas e matplotlib.
- Séries temporais completas de tensão e corrente existem apenas para o neurônio detalhado.
- Algumas métricas são derivadas após a execução, não durante o timestep neural.
- O único modelo neural implementado é o LIF simplificado.
- Há STDP aditivo experimental apenas para origens EXC; não há homeostase,
  recompensa, memória ou Worlds.

Veja também o [Mapa do projeto](MAPA_DO_PROJETO.md) e a
[Referência da API](../API_REFERENCE.md).

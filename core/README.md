# miniSNN

A miniSNN suporta LIF, AdEx e Hodgkin-Huxley em redes homogeneas. Veja
[Guia de modelos neuronais](docs/GUIA_DE_MODELOS_NEURONAIS.md).

Validacao C5: `mingw32-make test-adex`,
`mingw32-make test-hodgkin-huxley`, `mingw32-make scenario-adex-regular` e
`mingw32-make compare-neuron-models`.

A miniSNN Core é uma plataforma experimental em C para simulação, observação e
comparação de redes neurais pulsadas baseadas atualmente em um modelo LIF
simplificado.

O projeto é um protótipo funcional de laboratório experimental, com suíte
automatizada numérica, estrutural, de integração e regressão. Ele não é um
modelo biologicamente completo do cérebro nem uma plataforma cientificamente
validada.

## Estado atual

**Implementado:** neurônios LIF, pesos EXC/INH, delays sinápticos, corrente com
decaimento, oito topologias configuráveis, API pública opaca, cenários INI,
Studio Win32, CSVs, gráficos, comparação, histórico e diagnóstico
`off/basic/full`, inspeção de conexões, STDP aditivo por traces para sinapses
de origem EXC, homeostase opcional de threshold, scaling EXC e ganho INH, e
R-STDP opcional com elegibilidade e sinal externo de recompensa/punição, e
neuroevolução C3 serial com topologia fixa, fitness configurável e checkpoint.

**Experimental:** métricas de regime, sincronia aproximada e `stability_score`.
O STDP do C1 também é uma regra experimental simplificada. Esses recursos não
são verdades biológicas nem prova de aprendizado de tarefa.

**Estado da v0.2:** auditoria automática concluída; revisão manual do Studio e
revisão humana de release permanecem pendentes. O C1 foi implementado sobre
essa base; C1.5, C2 e C3 foram implementados localmente. miniSNN Worlds e
evolução estrutural ainda não estão implementados.

## Início rápido

Requisitos principais: Windows, ambiente MSYS2 UCRT64, GCC/MinGW e
`mingw32-make`. Python com pandas e matplotlib é opcional para análises e
gráficos.

```powershell
mingw32-make clean
mingw32-make test
mingw32-make studio-build
.\build\minisnn_studio.exe
```

Para executar um cenário pelo terminal:

```powershell
mingw32-make scenario-random
python scripts/analyze_run.py results/scenarios/random_demo --level basic
```

Resultados ficam em `results/scenarios/<actual_run_name>/`. Veja o
[Guia rápido](docs/GUIA_RAPIDO.md) para o fluxo completo de cinco minutos.

Relatórios locais legíveis podem ser gerados sem internet, mantendo os CSVs
como dados brutos:

```powershell
mingw32-make report-metrics RUN=results/scenarios/random_demo
mingw32-make report-weights RUN=results/scenarios/stdp_ltp_demo
mingw32-make report-history
```

Os comandos criam `metrics_report.html` e `weights_report.html` dentro da
própria run. No Studio, `ABRIR METRICAS` e `ABRIR PESOS` abrem esses HTMLs no
navegador padrão; os links internos continuam dando acesso a `metrics.csv`,
`weights_final.csv` e aos demais artefatos científicos.

`report-history` gera `results/scenarios/history.html` a partir do
`index.csv` append-only. O HTML funciona sem internet, mostra as execuções mais
recentes primeiro e oferece busca, filtros e links para artefatos existentes.

Para executar os cenários de aprendizado modulado por recompensa:

```powershell
mingw32-make scenario-reward-positive
mingw32-make scenario-punishment-negative
mingw32-make scenario-reward-delayed
mingw32-make scenario-reward-mixed
```

Cada run ativa gera CSVs de reward, `reward_report.txt`,
`reward_report.html` e `reward_overview.png`.

## Neuroevolução

O C3 evolui pesos iniciais e parâmetros escalares mantendo topologia e delays
fixos. O motor é serial, determinístico e usa herança darwiniana: pesos
aprendidos por STDP/R-STDP durante a vida não substituem o genoma inicial.

```powershell
mingw32-make evolution-build
mingw32-make evolution-weight-demo
mingw32-make plot-evolution RUN=results/evolution/evolution_weight_target_demo
mingw32-make report-evolution RUN=results/evolution/evolution_weight_target_demo
```

Veja o [Guia de neuroevolução](docs/GUIA_DE_NEUROEVOLUCAO.md).

## Topologia adaptativa

O C4 acrescenta dois mecanismos opt-in: evolução estrutural herdável e
plasticidade estrutural durante a vida. Neurônios e tipos EXC/INH permanecem
fixos; arestas, magnitudes e delays podem variar dentro de limites validados.

```powershell
mingw32-make evolution-structure-demo
mingw32-make structural-pruning-demo
mingw32-make structural-growth-demo
mingw32-make evolution-structure-learning-demo
```

O modo C3 `fixed_numeric` permanece padrão. Veja o
[Guia de topologia adaptativa](docs/GUIA_DE_TOPOLOGIA_ADAPTATIVA.md).

No Studio, `ABRIR EVENTOS ESTRUTURAIS` gera e abre
`structural_events_report.html`, enquanto `ABRIR MELHOR TOPOLOGIA` gera e abre
`best_topology_report.html`. Os CSVs `structural_events.csv` e
`best_topology.csv` continuam sendo os dados cientificos brutos.

## Memoria de trabalho temporal

O C6.1 adiciona um protocolo opt-in de `cue -> delay -> probe` para medir
retencao pela dinamica da rede, sem copiar o cue para a resposta. Execute o
demo deterministico com:

```powershell
mingw32-make scenario-working-memory
```

Os resultados incluem `working_memory_trials.csv`,
`working_memory_summary.txt` e `working_memory_report.html` na pasta normal da
run. O demo usa duas assembleias recorrentes com inibicao cruzada e registra
acaso, controle de rotulos embaralhados e margem de retencao. Veja o [Guia de
memoria de trabalho](docs/GUIA_DE_MEMORIA_DE_TRABALHO.md).

## Estrutura

```text
include/       API pública
src/           núcleo neural interno
app/           parser, runner e Studio
configs/       cenários reproduzíveis
examples/      exemplos da API e demo interno
experiments/   experimentos científicos exploratórios
scripts/       gráficos, métricas e comparação
tests/         testes automatizados
results/       saídas locais e resultados preservados
docs/          documentação
```

O fluxo e as responsabilidades estão em
[Arquitetura do Core](docs/ARQUITETURA_DO_CORE.md) e
[Mapa do projeto](docs/MAPA_DO_PROJETO.md).

## Documentação

Comece pelo [Índice da documentação](docs/INDICE_DA_DOCUMENTACAO.md). Ele
organiza os guias por público e finalidade.

Referências diretas:

- [Manual de uso](docs/MANUAL_DE_USO.md)
- [Guia do Studio](docs/GUIA_DO_STUDIO.md)
- [Guia de cenários](docs/GUIA_DE_CENARIOS.md)
- [Guia de diagnóstico](docs/GUIA_DE_DIAGNOSTICO.md)
- [Guia de plasticidade](docs/GUIA_DE_PLASTICIDADE.md)
- [Guia de homeostase](docs/GUIA_DE_HOMEOSTASE.md)
- [Guia de recompensa](docs/GUIA_DE_RECOMPENSA.md)
- [Guia de neuroevolução](docs/GUIA_DE_NEUROEVOLUCAO.md)
- [Guia de topologia adaptativa](docs/GUIA_DE_TOPOLOGIA_ADAPTATIVA.md)
- [Guia de memoria de trabalho](docs/GUIA_DE_MEMORIA_DE_TRABALHO.md)
- [Referência da API pública](API_REFERENCE.md)
- [Roadmap](docs/ROADMAP.md)

## Testes

```powershell
mingw32-make test
mingw32-make test-docs
mingw32-make test-diagnostics
mingw32-make test-plot-neuron
mingw32-make test-compare-runs
mingw32-make test-regression
mingw32-make test-analyzer
mingw32-make test-long
mingw32-make test-plasticity-long
mingw32-make test-plot-plasticity
mingw32-make test-homeostasis
mingw32-make test-homeostasis-long
mingw32-make test-plot-homeostasis
mingw32-make test-reward
mingw32-make test-reward-long
mingw32-make test-plot-reward
mingw32-make test-run-reports
mingw32-make test-history-report
mingw32-make check-c1
mingw32-make check-c15
mingw32-make check-c2
mingw32-make test-evolution
mingw32-make test-evolution-runner
mingw32-make test-evolution-long
mingw32-make test-plot-evolution
mingw32-make test-evolution-report
mingw32-make check-c3
mingw32-make test-structure
mingw32-make test-structural-plasticity
mingw32-make test-structure-resume
mingw32-make test-structure-long
mingw32-make test-plot-topology
mingw32-make benchmark-c4
mingw32-make check-c4
mingw32-make check-v02
```

`mingw32-make test` cobre API, núcleo, LIF numérico, parser, runner, topologias,
reprodutibilidade e estresse de memória. Os testes
Python e o validador documental são alvos separados. O escopo real de cada
teste está em [Princípios de desenvolvimento](docs/PRINCIPIOS_DE_DESENVOLVIMENTO.md).

## Limitações atuais

- O único modelo neural é o LIF simplificado, sem período refratário explícito.
- O STDP é aditivo, baseado em emissão e limitado a sinapses de origem EXC.
- A homeostase é um controle simplificado e opcional; não garante estabilidade.
- O reward é um escalar externo; não há política, agente, reward prediction error ou garantia de aprendizado de tarefa.
- Não há memória, criação/remoção de neurônios, NEAT ou speciation. O C4 varia
  apenas arestas entre neurônios fixos.
- A neuroevolução C3 otimiza apenas a fitness configurada; não garante ótimo global, inteligência geral ou convergência.
- O Studio depende da API Win32 e o fluxo principal de build foi validado no Windows.
- Diagnóstico completo depende de Python, pandas e matplotlib.
- Séries completas de tensão e corrente são gravadas para o neurônio detalhado, não para toda a rede.
- Unidades físicas não são impostas pela API; `dt`, pesos e correntes são parâmetros do cenário.

## Desenvolvimento e ciência

Toda feature que altera comportamento deve ter evidência verificável, modo de
reprodução, teste adequado e documentação proporcional ao impacto. Métricas
heurísticas são identificadas como tais e resultados não estabelecem causalidade
sem experimento apropriado.

A evidência e seus limites estão na
[Auditoria do Core v0.2](docs/AUDITORIA_DO_CORE_V02.md), na
[Cobertura de testes](docs/COBERTURA_DE_TESTES.md) e no
[roadmap oficial](docs/ROADMAP.md).

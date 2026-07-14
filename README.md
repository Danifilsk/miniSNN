# miniSNN

A miniSNN Core é uma plataforma experimental em C para simulação, observação e
comparação de redes neurais pulsadas baseadas atualmente em um modelo LIF
simplificado.

O projeto é um protótipo funcional de laboratório experimental, com suíte
automatizada numérica, estrutural, de integração e regressão. Ele não é um
modelo biologicamente completo do cérebro nem uma plataforma cientificamente
validada.

## Estado atual

**Implementado:** neurônios LIF, pesos EXC/INH, delays sinápticos, corrente com
decaimento, sete topologias configuráveis, API pública opaca, cenários INI,
Studio Win32, CSVs, gráficos, comparação, histórico e diagnóstico
`off/basic/full`, inspeção de conexões e STDP aditivo por traces para sinapses
de origem EXC.

**Experimental:** métricas de regime, sincronia aproximada e `stability_score`.
O STDP do C1 também é uma regra experimental simplificada. Esses recursos não
são verdades biológicas nem prova de aprendizado de tarefa.

**Estado da v0.2:** auditoria automática concluída; revisão manual do Studio e
revisão humana de release permanecem pendentes. O C1 foi implementado sobre
essa base; homeostase, recompensa e miniSNN Worlds ainda não estão implementados.

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
```

Os comandos criam `metrics_report.html` e `weights_report.html` dentro da
própria run. No Studio, `ABRIR METRICAS` e `ABRIR PESOS` abrem esses HTMLs no
navegador padrão; os links internos continuam dando acesso a `metrics.csv`,
`weights_final.csv` e aos demais artefatos científicos.

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
mingw32-make test-run-reports
mingw32-make check-c1
mingw32-make check-v02
```

`mingw32-make test` cobre API, núcleo, LIF numérico, parser, runner, topologias,
reprodutibilidade e estresse de memória. Os testes
Python e o validador documental são alvos separados. O escopo real de cada
teste está em [Princípios de desenvolvimento](docs/PRINCIPIOS_DE_DESENVOLVIMENTO.md).

## Limitações atuais

- O único modelo neural é o LIF simplificado, sem período refratário explícito.
- O STDP é aditivo, baseado em emissão e limitado a sinapses de origem EXC.
- Não há homeostase, recompensa, memória ou topologia adaptativa.
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

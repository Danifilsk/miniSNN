# Princípios de desenvolvimento

## Lei da miniSNN

1. Teste automatizado.
2. Configuração ou cenário reproduzível.
3. Saída observável — CSV, log ou gráfico conforme fizer sentido.
4. Controle no Studio quando útil.
5. Documentação curta e nota de compatibilidade.

Essa lei não exige que toda função interna gere um gráfico. A interpretação é:

> Toda feature que altera comportamento deve possuir evidência verificável,
> modo de reprodução, teste adequado e documentação proporcional ao impacto.

## Compatibilidade

- Preservar configs antigas quando possível.
- Preservar a API pública e deprecar antes de remover.
- Não alterar resultados dinâmicos sem justificativa e teste de regressão.
- Registrar mudanças de formato, fórmula ou semântica.
- Distinguir nome solicitado (`run_name`) de pasta efetiva (`actual_run_name`).

## Tipos de teste

- **Numérico:** compara um valor matemático com resultado esperado.
- **Estrutural:** verifica listas, índices, arquivos e invariantes.
- **Integração:** atravessa mais de um módulo ou processo.
- **Regressão:** preserva comportamento conhecido após mudança.
- **Smoke test:** confirma que um fluxo executa sem aprofundar toda a matemática.

Nem toda feature possui hoje todos esses níveis.

## Testes atuais

| Teste | O que valida hoje |
|---|---|
| `test_minisnn_api` | Criação, destruição, entradas, conexões e consultas públicas. |
| `test_topology` | Núcleo interno, topologias, estímulos, recorders e casos de memória analisáveis pelo compilador. |
| `test_LIF` | Fórmula discreta independente, trajetória, limiar, reset, spike exato e entrada negativa. |
| `test_scenario_config` | Parser, defaults, rejeições e compatibilidade de configs antigas. |
| `test_scenario_runner` | Topologias pelo runner, arquivos, histórico, nome único e diagnóstico básico/off. |
| `test_plot_neuron` | Geração do gráfico individual a partir de CSV sintético. |
| `test_compare_runs` | Comparação, fallback legado, métricas armazenadas e nomes únicos. |
| `test_analyze_run` | Valores exatos de atividade, burst, distribuição, EXC/INH, ISI e neurônio detalhado. |
| `test_metrics_common` | Gini, entropia, burst, correlação, estabilidade e regimes. |
| `test_runner_topologies` | Estrutura exata das sete topologias do runner/Studio. |
| `test_reproducibility` | Determinismo por seed de estrutura, CSVs e dinâmica. |
| `test_regression_baseline` | Golden pequeno e resultados históricos. |
| `test_memory_stress` / `test_long` | Ciclo de vida, rede densa, delays e execução longa. |
| `test_docs` | Links, referências, alvos, chaves e navegação documental. |

O escopo e as lacunas atuais estão em [Cobertura de testes](COBERTURA_DE_TESTES.md).
Sanitizers não foram executados neste MinGW por ausência dos runtimes.

## Observabilidade

Uma feature deve deixar evidência apropriada: CSV, relatório, log, métrica,
gráfico ou estado consultável. A forma depende do comportamento observado e do
custo aceitável.

## Documentação de uma feature

Deve informar o que faz, como ativar, como validar, quais arquivos produz,
limitações e impacto de compatibilidade.

## Escopo científico e linguagem

- Não chamar heurística de verdade biológica.
- Não afirmar causalidade sem experimento adequado.
- Não chamar protótipo de modelo cerebral completo.
- Distinguir simulação, observação, interpretação e hipótese.
- Informar quando um resultado é experimental ou ainda está em validação.

## Mudanças adaptativas

Plasticidade deve declarar regra, elegibilidade, ordem temporal e limites. No C1,
STDP é aditivo, usa emissão do spike, altera apenas origens EXC e mantém INH
fixa. Um peso maior é uma observação da execução, não evidência automática de
aprendizado. Módulos futuros como homeostase não podem ser presumidos.

## Uso de ferramentas de IA

Ferramentas de IA generativa foram utilizadas como apoio à implementação,
refatoração e documentação. As funcionalidades são submetidas a testes,
execuções reproduzíveis e revisão dos resultados. A ferramenta não é autora do
projeto e esta nota não substitui as regras da instituição ou publicação futura.

## Controles adaptativos

Homeostase é opt-in e deve preservar bit a bit os golden tests quando desligada.
Cada mecanismo precisa de fórmula única no Core, ordem temporal explícita,
estatística separada do STDP, limites configuráveis e linguagem científica que
não prometa estabilidade universal.

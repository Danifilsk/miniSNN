# Índice da documentação

Este é o ponto central de navegação da miniSNN. O estado atual distingue recursos
implementados, experimentais e planejados.

## Para começar

- [README](../README.md) — visão curta do projeto, estado atual e comandos iniciais.
- [Guia rápido](GUIA_RAPIDO.md) — compilação e primeira execução em cerca de cinco minutos.
- [Manual de uso](MANUAL_DE_USO.md) — instalação, comandos, solução de problemas e fluxos completos.
- [Guia do Studio](GUIA_DO_STUDIO.md) — uso da interface gráfica Win32.

## Para executar experimentos

- [Guia de cenários](GUIA_DE_CENARIOS.md) — chaves INI, topologias e execução reproduzível.
- [Guia de modelos neuronais](GUIA_DE_MODELOS_NEURONAIS.md) — LIF, AdEx, Hodgkin-Huxley e limites de interpretação.
- [Guia de experimentos](GUIA_DE_EXPERIMENTOS.md) — programas científicos exploratórios preservados.
- [Organização de resultados](ORGANIZACAO_DE_RESULTADOS.md) — pastas, `index.csv` append-only e `history.html` local.
- [Guia de métricas](GUIA_DE_METRICAS.md) — leitura das métricas principais.
- [Guia de diagnóstico](GUIA_DE_DIAGNOSTICO.md) — níveis, fórmulas, regimes e limitações heurísticas.
- [Guia de plasticidade](GUIA_DE_PLASTICIDADE.md) — STDP, traces, cenários, saídas e limitações.
- [Guia de homeostase](GUIA_DE_HOMEOSTASE.md) — threshold, scaling EXC, ganho INH e ordem temporal.
- [Guia de recompensa](GUIA_DE_RECOMPENSA.md) — R-STDP, elegibilidade, punição, eventos e outputs.
- [Guia de neuroevolução](GUIA_DE_NEUROEVOLUCAO.md) — genomas, fitness, operadores, checkpoint, Studio e limites C3.
- [Guia de topologia adaptativa](GUIA_DE_TOPOLOGIA_ADAPTATIVA.md) — genoma estrutural, patches, lifetime plasticity, outputs e limites C4.
- [Guia de memoria de trabalho](GUIA_DE_MEMORIA_DE_TRABALHO.md) — protocolo temporal C6.1, metricas de recall, outputs e limites.
- [Guia de memoria associativa](GUIA_DE_MEMORIA_ASSOCIATIVA.md) — treino STDP cue-alvo, recall parcial, controles e limites C6.2.

Os guias de diagnóstico e plasticidade também explicam os relatórios locais
`metrics_report.html` e `weights_report.html`. Eles são a camada de leitura;
os CSVs permanecem como fonte bruta e podem ser acessados pelos links do HTML.

## Para entender tecnicamente

- [Arquitetura do Core](ARQUITETURA_DO_CORE.md) — camadas, encapsulamento e fluxo de dados.
- [Mapa do projeto](MAPA_DO_PROJETO.md) — responsabilidade de cada arquivo e pasta principal.
- [Referência da API](../API_REFERENCE.md) — assinaturas públicas atuais de `minisnn.h`.
- [Glossário](GLOSSARIO.md) — termos de SNN, cenários e diagnóstico.

## Para desenvolver

- [Princípios de desenvolvimento](PRINCIPIOS_DE_DESENVOLVIMENTO.md) — lei da miniSNN, testes e linguagem científica.
- [Compatibilidade](COMPATIBILIDADE.md) — configs antigas, outputs, API e plataforma.
- [Matriz de rastreabilidade](MATRIZ_DE_RASTREABILIDADE.md) — feature, implementação, teste e evidência.
- [Auditoria do Core v0.2](AUDITORIA_DO_CORE_V02.md) — evidências, bugs, limites e estado da fundação.
- [Cobertura de testes](COBERTURA_DE_TESTES.md) — classificação e lacunas reais da suíte.
- [Benchmarks v0.2](BENCHMARKS_V02.md) — medições locais reproduzíveis e limites.
- [Benchmarks C1 STDP](BENCHMARKS_C1_STDP.md) — custo local da regra, histórico e gráfico.
- [Benchmarks C1.5](BENCHMARKS_C15_HOMEOSTASE.md) — custo local dos mecanismos e histórico.
- [Benchmarks C2](BENCHMARKS_C2_RECOMPENSA.md) — custo de elegibilidade, reward, histórico e relatórios.
- [Benchmarks C3](BENCHMARKS_C3_NEUROEVOLUCAO.md) — custo local de avaliações, réplicas, serialização e relatórios.
- [Benchmarks C4](BENCHMARKS_C4_TOPOLOGIA.md) — custo local de evolução e manutenção estrutural.
- [Checklist do Studio](CHECKLIST_DE_VALIDACAO_DO_STUDIO.md) — validação manual separada da automação.
- [Roadmap](ROADMAP.md) — implementado, próximo e planejado, sem prazos garantidos.

## Status dos documentos

Os documentos deste índice descrevem o repositório atual. Funcionalidades futuras
aparecem somente quando marcadas como **PLANEJADO**. Resultados e métricas
exploratórias não equivalem a validação biológica.

# Glossário

- **SNN:** rede neural pulsada, na qual eventos discretos chamados spikes carregam atividade.
- **Spike:** evento produzido quando o potencial cruza o limiar do modelo.
- **Timestep:** passo discreto executado por `minisnn_step()`.
- **LIF:** Leaky Integrate-and-Fire, modelo simplificado com integração, vazamento, limiar e reset.
- **Potencial de membrana:** estado `V` integrado pelo neurônio LIF.
- **Limiar:** valor de potencial que dispara um spike.
- **Reset:** valor atribuído ao potencial após o spike.
- **Corrente externa:** entrada aplicada pelo usuário ou cenário.
- **Corrente sináptica:** contribuição recebida por conexões e usada no update LIF.
- **Neurônio excitatório (EXC):** tipo cuja saída usa peso positivo nos cenários balanceados.
- **Neurônio inibitório (INH):** tipo cuja saída usa peso negativo nos cenários balanceados.
- **Peso sináptico:** magnitude e sinal da corrente produzida por uma conexão.
- **Delay:** atraso inteiro, em timesteps, antes da chegada da corrente.
- **Topologia:** regra de criação das conexões.
- **Chain:** cadeia dirigida `0 -> 1 -> ...`.
- **Ring:** cadeia com conexão de retorno do último ao primeiro.
- **All-to-all:** todos os pares permitidos são conectados.
- **Random:** pares são aceitos por probabilidade e gerador determinístico do runner.
- **Random balanced:** random com tipos EXC/INH definidos pela fração inibitória.
- **Small-world:** vizinhança circular com rewiring probabilístico.
- **Feedforward:** camadas consecutivas sem conexões de retorno pelo construtor atual.
- **Seed:** valor inicial do gerador pseudoaleatório determinístico.
- **Raster:** tabela de spikes com timestep, neurônio e tipo.
- **Burst:** grupo de passos consecutivos acima do threshold configurado.
- **ISI:** intervalo entre spikes consecutivos de um neurônio.
- **Fano factor:** variância dividida pela média segundo a série ou janela usada.
- **Coeficiente de variação:** desvio padrão dividido pela média.
- **Gini:** desigualdade da contagem de spikes entre neurônios.
- **Entropia:** medida de distribuição da atividade; a versão normalizada facilita comparação.
- **Sincronia aproximada:** proxy baseado em concentração temporal, não sincronia biológica completa.
- **Stability score:** índice heurístico da miniSNN entre 0 e 1.
- **Regime diagnóstico:** rótulo heurístico como `silent`, `sustained` ou `mixed`.
- **run_name:** nome solicitado na configuração.
- **actual_run_name:** nome efetivo da pasta, possivelmente com timestamp.
- **Manifesto:** `run_manifest.txt`, que registra proveniência e ferramentas.
- **Diagnostics off/basic/full:** níveis de custo e profundidade do diagnóstico.
- **Heurística:** regra útil para diagnóstico, sem garantia de verdade biológica universal.
- **Reprodutibilidade:** capacidade de repetir configuração, seed, código e procedimento observável.

Fórmulas completas estão no [Guia de diagnóstico](GUIA_DE_DIAGNOSTICO.md) e no
[Guia de métricas](GUIA_DE_METRICAS.md).

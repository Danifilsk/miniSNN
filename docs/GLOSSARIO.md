# Glossário

**Recompensa:** sinal escalar externo positivo que modula elegibilidades no R-STDP.

**Punição:** reward negativo; inverte matematicamente a consequência de uma elegibilidade.

**Reforço:** aplicação de um sinal modulador a uma correlação ainda elegível.

**R-STDP:** STDP modulado por recompensa; a correlação cria elegibilidade e o reward altera o peso.

**Aprendizado de três fatores:** regra que combina atividade pré, atividade pós e sinal modulador.

**Trace de elegibilidade:** memória sináptica temporária e decadente da correlação pré/pós.

**Reward atrasado:** sinal aplicado em step posterior ao que criou a elegibilidade.

**Sinal modulador:** valor global externo que multiplica elegibilidades.

**Reward pendente:** soma bruta enfileirada para consumo no próximo step.

**Reward aplicado:** valor após agregação e eventual clamp, consumido uma única vez.

**Mudança de peso modulada:** `learning_rate * reward * eligibility`, antes do clamp do peso.

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
- **Plasticidade sináptica:** mudança do peso de uma conexão durante a execução.
- **STDP:** regra que relaciona deltas de peso à ordem temporal de spikes pré e pós.
- **LTP:** termo de potencialização, com delta positivo na regra do C1.
- **LTD:** termo de depressão, com delta negativo na regra do C1.
- **Trace pré-sináptico:** memória exponencial dos spikes anteriores do neurônio pré.
- **Trace pós-sináptico:** memória exponencial dos spikes anteriores do neurônio pós.
- **Janela temporal:** influência da distância entre spikes, determinada pelos taus.
- **Clamp de peso:** limitação do peso ao intervalo configurado.
- **Sinapse elegível:** conexão de origem EXC, peso não negativo e regra ativa no C1.

Fórmulas completas estão no [Guia de diagnóstico](GUIA_DE_DIAGNOSTICO.md) e no
[Guia de métricas](GUIA_DE_METRICAS.md).

## Homeostase

- **Homeostase neural:** controle simplificado que tenta aproximar atividade de uma referência.
- **Taxa-alvo:** parâmetro do modelo usado como referência, não constante biológica universal.
- **Rate trace:** média exponencial de spikes por unidade temporal.
- **Threshold adaptativo:** limiar individual alterado a partir do erro de taxa.
- **Excitabilidade:** facilidade relativa com que o neurônio alcança o limiar.
- **Escalonamento sináptico:** multiplicação coordenada das entradas EXC.
- **Soma de entrada excitatória:** soma dos pesos positivos de origens EXC para um destino.
- **Ganho inibitório:** multiplicador global aplicado na transmissão INH sem mudar o peso bruto.
- **Erro de taxa:** taxa observada menos taxa-alvo.
- **Clamp homeostático:** limite mínimo/máximo aplicado a threshold, peso, fator ou ganho.
- **Neuroevolução:** otimização experimental de parâmetros neurais por operadores evolutivos.
- **Indivíduo:** um genoma avaliado em uma rede reconstruída.
- **População:** conjunto de indivíduos de uma geração.
- **Genoma:** vetor ordenado de valores herdáveis.
- **Fenótipo:** rede e dinâmica produzidas ao aplicar um genoma ao blueprint.
- **Gene:** posição do genoma com tipo, bounds e baseline.
- **Fitness:** adequação numérica somente ao objetivo configurado.
- **Seleção:** escolha de indivíduos para reprodução.
- **Torneio:** seleção pelo melhor de uma amostra sem reposição.
- **Elitismo:** cópia dos melhores genomas para a próxima geração.
- **Crossover:** combinação de genes de dois pais.
- **Mutação:** alteração aleatória limitada de genes.
- **Geração:** ciclo de avaliação e reprodução de uma população.
- **Linhagem:** relação registrada entre indivíduo, pais e operação.
- **Blueprint estrutural:** topologia, tipos e delays fixos usados por todos os indivíduos.
- **Diversidade genética:** dispersão dos valores dos genes na população.
- **Checkpoint evolutivo:** estado completo salvo numa fronteira de geração.
- **Herança darwiniana:** herança do genoma inicial, não das mudanças adquiridas na vida.
- **Herança lamarckiana:** herança de mudanças adquiridas; desativada no C3.
- **Convergência prematura:** perda de diversidade antes de explorar soluções suficientes.

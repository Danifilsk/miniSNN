# Guia de plasticidade sináptica

## Modos de aprendizagem no C2

`learning_mode = direct_stdp` preserva a atualização imediata deste guia.
`learning_mode = reward_modulated_stdp` usa a mesma correlação pré/pós como
incremento de elegibilidade e só modifica pesos ao consumir reward. Não há dois
caminhos simultâneos. Consulte [Recompensa e R-STDP](GUIA_DE_RECOMPENSA.md)
para fórmulas, API, eventos, outputs e limitações.

O Bloco C1 adiciona à miniSNN uma primeira regra experimental de plasticidade:
STDP aditivo baseado em pares e traces exponenciais. A implementação é
simplificada, não representa toda a plasticidade biológica e não demonstra
aprendizado de uma tarefa apenas porque um peso aumentou.

## 1. Conceitos

Plasticidade sináptica é a mudança do peso de uma conexão ao longo da
simulação. STDP relaciona essa mudança à ordem temporal dos spikes pré e
pós-sinápticos. No C1, LTP indica um delta positivo e LTD um delta negativo.

Cada neurônio possui dois traces independentes, inicialmente zero:

```text
x_pre[i]
x_post[i]
```

Os traces decaem exponencialmente. `tau_plus` e `tau_minus` usam a mesma unidade
temporal de `dt`; a miniSNN não impõe automaticamente uma unidade física.

## 2. Regra implementada

Para uma conexão elegível `i -> j`, depois do decaimento dos traces:

```text
delta_w = spike[j] * a_plus * x_pre[i]
        - spike[i] * a_minus * x_post[j]

weight = clamp(weight + delta_w, weight_min, weight_max)
```

Depois de aplicar todos os deltas do timestep:

```text
x_pre[i]  += trace_increment * spike[i]
x_post[i] += trace_increment * spike[i]
```

`a_plus` e `a_minus` têm unidade de peso por unidade de trace. A regra soma LTP
e LTD antes de aplicar um único clamp, inclusive em uma autoconexão. Spikes
simultâneos sem histórico anterior produzem delta zero.

## 3. Ordem temporal

Em cada timestep:

1. o LIF usa as entradas atuais;
2. spikes são transmitidos e agendados com o peso anterior;
3. os traces anteriores decaem;
4. LTP e LTD são calculados com os mesmos traces decaídos;
5. o delta combinado é aplicado e limitado;
6. os traces dos spikes atuais são incrementados.

O peso novo afeta somente transmissões futuras. O evento pré-sináptico do C1 é
o instante de **emissão** do spike, não sua chegada depois do delay. Delays
continuam afetando a transmissão normal da rede.

## 4. Elegibilidade

Uma conexão é plástica quando STDP está ativo, a regra é
`stdp_pair_trace`, a origem é EXC e o peso da conexão é finito e não negativo.
Sinapses de origem INH permanecem fixas, negativas e aparecem com
`eligible = 0`. Plasticidade inibitória e STDP baseado em chegada não fazem
parte do C1.

## 5. Configuração INI

```ini
[plasticity]
enabled = false
rule = stdp_pair_trace
a_plus = 1.0
a_minus = 1.05
tau_plus = 20.0
tau_minus = 20.0
trace_increment = 1.0
weight_min = 0.0
weight_max = 200.0
record_weights = true
record_history = true
record_interval_steps = 10
record_connection_limit = 256
```

Os defaults são de engenharia, não parâmetros biológicos universais. `a_plus`
e `a_minus` devem ser finitos e não negativos; taus e incremento devem ser
positivos; `weight_min >= 0`; `weight_max > weight_min`; intervalo e limite
devem ser inteiros positivos. `rule` aceita apenas `stdp_pair_trace` no C1.

Configs antigas sem a seção e configs com `enabled = false` mantêm STDP
desligado. Alterar a configuração pública preserva os pesos atuais, zera traces
e estatísticas e reconstrói o índice necessário. Conexões não podem ser
adicionadas depois do primeiro step enquanto STDP estiver ativo.

## 6. Cenários reproduzíveis

```powershell
mingw32-make scenario-stdp-ltp
mingw32-make scenario-stdp-ltd
mingw32-make scenario-stdp-mixed
```

- `configs/stdp_ltp_demo.ini`: demonstra aumento líquido do peso.
- `configs/stdp_ltd_demo.ini`: demonstra redução líquida do peso.
- `configs/stdp_mixed_demo.ini`: combina EXC/INH e mantém origens INH fixas.

Esses cenários demonstram o comportamento da regra nesta configuração; não
provam aprendizado, memória funcional ou validade biológica.

## 7. Saídas

Com STDP ativo, o runner sempre gera `plasticity_metrics.csv` e
`stdp_report.txt`. Quando `record_weights = true`, gera
`weights_initial.csv` e `weights_final.csv`. Quando
`record_history = true`, gera `weight_history.csv` com step zero e estado final.

As métricas agregadas usam **todas** as conexões elegíveis. Os snapshots podem
ser limitados por `record_connection_limit`. Quando limitados, os IDs são
distribuídos deterministicamente entre `0` e `E-1`; `sampled = 1` informa que o
arquivo é uma amostra. Para limite 1, a conexão plana 0 é usada.

`plasticity_total_absolute_change` acumula o módulo das mudanças efetivamente
aplicadas ao longo dos timesteps. Ele não é necessariamente igual à soma de
`abs(final_weight - initial_weight)`, porque mudanças opostas podem se cancelar.
Casos não aplicáveis usam zero ou `NA` conforme o tipo do arquivo, nunca
`NaN`/infinito.

O gráfico é gerado separadamente:

```powershell
mingw32-make plot-stdp-ltp
python scripts/plot_plasticity.py results/scenarios/stdp_ltp_demo
```

O script cria `plasticity_overview.png` com distribuição inicial/final,
mudanças, trajetórias amostradas e métricas principais, e também gera
`weights_report.html`. O HTML mostra cards, tabela limitada para leitura,
rankings, aviso explícito de amostragem e links para os CSVs completos. Ele
funciona localmente e sem internet. Para gerar apenas o relatório:

```powershell
python scripts/generate_run_reports.py results/scenarios/stdp_ltp_demo --weights
```

`weights_final.csv` permanece como fonte bruta. Em redes grandes, o HTML exibe
até 500 registros e informa quantos foram omitidos apenas da apresentação; a
amostragem científica produzida pelo runner não é alterada.

## 8. API pública

`MiniSNNConnectionInfo` expõe uma visão segura da conexão. A enumeração plana é
determinística: origem crescente e ordem de inserção dentro da origem.

As funções públicas novas consultam contagem, conexão e peso; alteram peso
finito; configuram STDP; consultam configuração, estatísticas e traces. A API
completa está em [API_REFERENCE.md](../API_REFERENCE.md).

## 9. Studio

O botão `PLASTICIDADE` abre um modal com ON/OFF, regra, amplitudes, taus,
incremento, limites e opções de registro. `GRAFICO STDP` usa o Python já
resolvido pelo Studio; `ABRIR PESOS` abre `weights_report.html`; `ABRIR STDP`
abre o PNG da última pasta real. O relatório é gerado sob demanda quando
`weights_final.csv` existe. Com STDP OFF ou sem registro de pesos, o Studio
informa que não há relatório em vez de criar dados fictícios.

## 10. Testes e desempenho

```powershell
mingw32-make test-plasticity
mingw32-make test-plasticity-long
mingw32-make test-plot-plasticity
mingw32-make test-run-reports
mingw32-make test-regression
mingw32-make test-reproducibility
mingw32-make check-c1
```

Os testes cobrem valores exatos de LTP/LTD, intervalos, simultaneidade, clamps,
autoconexão, ordem transmissão/aprendizado, traces, INH fixa, parser, runner,
reprodutibilidade e execução longa. Medições locais estão em
[BENCHMARKS_C1_STDP.md](BENCHMARKS_C1_STDP.md).

## 11. Limitações

- regra aditiva simplificada e experimental;
- apenas sinapses de origem EXC são plásticas;
- referência temporal por emissão do spike;
- sem homeostase ou normalização de pesos;
- sem recompensa, elegibilidade, triplets ou plasticidade estrutural;
- aumento de peso não prova aprendizado de tarefa;
- parâmetros e resultados exigem interpretação científica cuidadosa.

## Relação com a homeostase C1.5

Quando ambos estão ativos, o STDP atualiza primeiro os pesos EXC elegíveis e o
scaling atua depois sobre o resultado. Estatísticas STDP contam somente STDP;
estatísticas homeostáticas contam scaling. `weights_final.csv` mostra o efeito
líquido e o relatório de pesos distingue os dois mecanismos. Consulte o
[Guia de homeostase](GUIA_DE_HOMEOSTASE.md).

## STDP dentro da neuroevolução

Genes podem definir pesos EXC iniciais e parâmetros `a_plus`, `a_minus`,
`tau_plus` e `tau_minus`. O STDP opera durante cada vida; seus pesos finais
afetam a fitness, mas a descendência recebe o genoma inicial. Compare
`best_run/weights_initial.csv` com `best_run/weights_final.csv`. Herança
lamarckiana está desativada no C3.

## STDP com mudanças estruturais

No C4, uma conexão sobrevivente preserva peso e usa os traces neuronais atuais;
uma removida deixa de participar; e uma nova começa com o peso configurado e só
atua em eventos futuros. O rebuild ocorre uma vez por patch. A topologia
fenotípica aprendida durante a vida influencia a fitness, mas não é herdada.

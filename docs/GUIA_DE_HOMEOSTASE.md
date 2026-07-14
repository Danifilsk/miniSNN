# Guia de homeostase

## Interação com R-STDP

Quando ambos estão ativos, o step transmite com pesos antigos, atualiza
elegibilidades, aplica o reward e só então executa scaling no intervalo
homeostático. `reward_metrics.csv` contabiliza apenas R-STDP;
`homeostasis_metrics.csv` contabiliza apenas scaling; `weights_final.csv`
mostra o efeito líquido. O reset de reward preserva todo o estado homeostático.

[Voltar ao índice](INDICE_DA_DOCUMENTACAO.md)

## 1. Objetivo e limites

O C1.5 oferece três mecanismos opcionais de controle da atividade: limiar
adaptativo, escalonamento das entradas excitatórias e ganho inibitório global.
Eles são simplificações de engenharia para experimentação. Não garantem
estabilidade, não representam toda a homeostase biológica e não tornam uma
configuração biologicamente validada.

Com `[homeostasis]` ausente ou `enabled = false`, a rede usa o limiar-base, o
ganho inibitório `1.0`, não aloca os vetores homeostáticos e mantém a dinâmica
anterior. Os resultados de regressão permanecem `random_demo = 6757` e
`small_world_demo = 15045` spikes.

## 2. Taxa estimada

Cada neurônio mantém um `rate trace` exponencial:

```text
alpha = exp(-dt / rate_tau)
r_next = alpha * r + (1 - alpha) * (spike / dt)
```

A unidade da taxa é spikes por unidade temporal de `dt`. O trace começa em
zero. A taxa populacional é a média dos traces individuais.

## 3. Limiar adaptativo

A cada `update_interval_steps` passos completos:

```text
threshold_next = clamp(
    threshold + threshold_eta * (rate_trace - target_rate),
    threshold_min,
    threshold_max)
```

Taxa acima da meta eleva numericamente o limiar e reduz a excitabilidade. Taxa
abaixo da meta reduz o limiar e aumenta a excitabilidade. O spike atual usa o
limiar anterior; a mudança só vale para passos futuros.

## 4. Escalonamento sináptico

O modo suportado é `initial_incoming_sum`. Ao configurar ou resetar, a rede
captura para cada destino a soma inicial `T` das entradas EXC positivas. Em uma
atualização, calcula a soma atual `S` e aplica:

```text
desired = T / S
factor = clamp(1 + scaling_eta * (desired - 1),
               scaling_min_factor,
               scaling_max_factor)
weight_next = clamp(weight * factor,
                    scaling_weight_min,
                    scaling_weight_max)
```

Somente pesos de origem EXC são alterados. O índice de entradas compartilhado
com o STDP guarda `source` e índice de saída, evitando ponteiros que poderiam
ser invalidados por `realloc`. O scaling preserva aproximadamente as proporções
relativas, mas clamps podem quebrar essa proporcionalidade.

Scaling não cria conexões e não recupera multiplicativamente um peso exatamente
zero. Quando `T > 0` e `S = 0`, registra um `scaling_zero_sum_skip` e não divide
por zero. Pesos INH brutos nunca são alterados pelo scaling.

## 5. Ganho inibitório

O ganho global segue:

```text
gain_next = clamp(
    gain + inhibitory_gain_eta * (population_rate - target_rate),
    inhibitory_gain_min,
    inhibitory_gain_max)
```

Na transmissão INH, o peso efetivo é `raw_weight * gain`. O peso bruto salvo em
`weights_initial.csv` e `weights_final.csv` permanece igual. A transmissão do
passo atual usa o ganho anterior; a atualização afeta apenas transmissões
futuras. Em redes sem conexões INH, o ganho ainda pode mudar sem afetar corrente.

## 6. Ordem temporal

```text
1. integrar o LIF com thresholds efetivos atuais
2. obter spikes
3. agendar transmissão com pesos e ganho atuais
4. executar STDP
5. atualizar rate traces
6. no intervalo: threshold, scaling e ganho
7. registrar o estado
8. usar os novos estados em passos futuros
```

Assim, o STDP produz a mudança correlacional local e o scaling atua depois,
sobre os pesos resultantes. Estatísticas STDP não contam scaling. O peso final é
o efeito líquido dos mecanismos ativos.

## 7. Configuração INI

```ini
[homeostasis]
enabled = true
intrinsic_enabled = true
target_rate = 0.05
rate_tau = 100.0
update_interval_steps = 10
threshold_eta = 0.05
threshold_min = -60.0
threshold_max = -40.0
synaptic_scaling_enabled = false
scaling_target_mode = initial_incoming_sum
scaling_eta = 0.10
scaling_min_factor = 0.50
scaling_max_factor = 2.00
scaling_weight_min = 0.0
scaling_weight_max = 1000.0
inhibitory_gain_enabled = false
inhibitory_gain_initial = 1.0
inhibitory_gain_eta = 0.05
inhibitory_gain_min = 0.25
inhibitory_gain_max = 4.0
record_history = true
record_interval_steps = 10
record_neuron_limit = 256
```

Valores devem ser finitos. Taus e intervalos são positivos; limites devem estar
ordenados; `scaling_eta` fica entre 0 e 1; ganhos são positivos. Com
`enabled = true`, ao menos um mecanismo deve estar ativo. O threshold-base da
rede deve estar dentro dos limites configurados.

## 8. Reset e API

`minisnn_set_homeostasis_config` e `minisnn_reset_homeostasis` preservam pesos,
potenciais, passo e estado STDP. Eles zeram traces e estatísticas, restauram os
thresholds ao limiar-base, restauram o ganho inicial e recapturam as somas EXC
usando os pesos atuais.

Os tipos `MiniSNNHomeostasisConfig` e `MiniSNNHomeostasisStats`, seus getters e
os getters por neurônio estão descritos em [API_REFERENCE.md](../API_REFERENCE.md).
Cada rede possui estado privado e redes independentes não compartilham traces,
thresholds, alvos, ganho ou estatísticas.

## 9. Saídas

Quando a homeostase está ativa, o runner pode produzir:

- `homeostasis_metrics.csv`: configuração, agregados, contadores e estado final.
- `homeostasis_history.csv`: taxa populacional, erro e ganho por amostra.
- `threshold_history.csv`: estatísticas dos thresholds por amostra.
- `homeostasis_neurons.csv`: amostra determinística de neurônios.
- `homeostasis_report.txt`: resumo textual local.
- `homeostasis_report.html`: relatório HTML local sem recursos externos.
- `homeostasis_overview.png`: panorama gerado por Python.

O último passo é sempre registrado, mesmo quando não é múltiplo do intervalo.
Para redes maiores, são selecionados até `record_neuron_limit` IDs em ordem
determinística e uniformemente espaçados.

## 10. Execução e gráficos

```powershell
mingw32-make scenario-homeostasis-silence
mingw32-make scenario-homeostasis-explosion
mingw32-make scenario-homeostasis-stdp
mingw32-make plot-homeostasis RUN=results/scenarios/homeostasis_stdp_scaling_demo
```

No Studio, `HOMEOSTASE` abre o modal de configuração. `GRAFICO HOMEOSTASE`
executa `scripts/plot_homeostasis.py` sobre a última run real e `ABRIR
HOMEOSTASE` abre o HTML local. Python com pandas e matplotlib é necessário para
o PNG, não para executar a simulação.

## 11. Cenários reproduzíveis

- `configs/homeostasis_silence_recovery_demo.ini`: threshold intrínseco em baixa atividade.
- `configs/homeostasis_explosion_control_demo.ini`: threshold e ganho em atividade alta.
- `configs/homeostasis_stdp_scaling_demo.ini`: STDP seguido por scaling EXC.

Os resultados demonstram o mecanismo para aquelas configurações; não provam
estabilidade geral nem causalidade biológica. Silêncio e hiperatividade ainda
podem ocorrer, e a aproximação da taxa-alvo depende dos parâmetros e da rede.

## 12. Testes e desempenho

```powershell
mingw32-make test-homeostasis
mingw32-make test-homeostasis-long
mingw32-make test-plot-homeostasis
mingw32-make benchmark-c15
mingw32-make check-c15
```

Os testes cobrem fórmulas, clamps, ordem temporal, reset, isolamento entre redes,
STDP com scaling, runner, execução prolongada, valores finitos, CSV, PNG e HTML.
O benchmark local está documentado em
[BENCHMARKS_C15_HOMEOSTASE.md](BENCHMARKS_C15_HOMEOSTASE.md).

## Homeostase dentro da neuroevolução

O C3 pode evoluir `target_rate`, `rate_tau`, `threshold_eta`, `scaling_eta` e
`inhibitory_gain_eta` quando o mecanismo correspondente está ativo no
cenário-base. Erro de taxa pode ser fitness. O resultado depende de bounds e
seeds; homeostase e evolução não garantem estabilidade universal.

# Guia de recompensa, punição e R-STDP

Este guia descreve o Bloco C2 da miniSNN: um aprendizado opcional de três
fatores. Ele combina atividade pré-sináptica, atividade pós-sináptica e um
sinal modulador escalar externo. O mecanismo é experimental e não representa
um agente, uma política ou compreensão de recompensa.

## 1. STDP direto e R-STDP

Em `direct_stdp`, a correlação temporal altera imediatamente o peso. Em
`reward_modulated_stdp`, a mesma correlação gera uma elegibilidade por conexão;
o peso só muda quando uma recompensa ou punição é aplicada. Os modos são
mutuamente exclusivos.

Configurações antigas sem `learning_mode` usam `direct_stdp`. Se `[reward]`
estiver ausente ou `enabled = false`, nenhuma elegibilidade é alocada e o
comportamento anterior é preservado.

## 2. Regra de elegibilidade

Para uma conexão excitatória `i -> j`, o candidato temporal reutilizado do C1 é:

```text
c_ij = s_j A_plus x_pre_i + s_i A_minus x_post_j
```

No modo R-STDP:

```text
e_ij(t) = clamp(e_ij(t0) exp(-(t - t0) dt / tau_e) + c_ij,
                eligibility_min, eligibility_max)
```

O decaimento é preguiçoso: cada conexão mantém a elegibilidade e o último step
em que ela foi materializada. Eventos pré/pós, reward, registro e getters
atualizam o valor por um decaimento matematicamente equivalente. Isso evita
percorrer todas as conexões em steps sem necessidade.

`eligibility` tem a unidade da mudança candidata de peso do C1. `reward` e
`learning_rate` são escalares adimensionais nesta implementação.

## 3. Recompensa e punição

Quando o sinal aplicado é `r`:

```text
delta_w_reward = learning_rate * r * e_ij
w_ij = clamp(w_ij + delta_w_reward, weight_min, weight_max)
```

- reward positivo reforça elegibilidade positiva e deprime elegibilidade negativa;
- punição é reward negativo e inverte essas consequências;
- reward zero não muda pesos;
- a elegibilidade não é zerada após o reward e continua decaindo;
- somente conexões de origem EXC são elegíveis; conexões de origem INH ficam fixas.

O sinal é externo. Não há reward prediction error, baseline adaptativo,
ator-crítico, seleção de ações, política ou aprendizado por tarefa garantido.

## 4. Ordem temporal

Em cada step, a miniSNN:

1. integra os neurônios com estados e thresholds atuais;
2. transmite spikes com o peso e o ganho INH anteriores;
3. atualiza traces pré/pós e candidatos temporais;
4. acumula candidatos em elegibilidade no modo R-STDP;
5. aplica e consome o reward pendente;
6. atualiza traces de taxa;
7. executa threshold adaptativo, scaling EXC e ganho INH no intervalo configurado;
8. registra históricos.

Portanto, o reward do step `k` vê a correlação criada no próprio step `k`, mas
o spike desse step usa o peso antigo. Se scaling estiver ativo, a ordem é
R-STDP e depois scaling. As estatísticas de cada mecanismo permanecem separadas.

## 5. Configuração INI

Em `[plasticity]`:

```ini
enabled = true
learning_mode = reward_modulated_stdp
```

Em `[reward]`:

```ini
enabled = true
mode = rstdp
learning_rate = 1.0
eligibility_tau = 100.0
eligibility_min = -200.0
eligibility_max = 200.0
reward_min = -1.0
reward_max = 1.0
clip_reward = true
record_history = true
record_interval_steps = 10
record_connection_limit = 256
```

`clip_reward = true` acumula o sinal bruto e limita uma vez antes da aplicação.
Com `false`, uma chamada que sairia dos bounds é recusada sem estado parcial.
Os limites de elegibilidade devem conter zero; `eligibility_tau` é positivo;
`learning_rate` é finito e não negativo.

Eventos agendados ficam em `[reward_events]`:

```ini
event_0 = 300,1.0
event_1 = 600,-1.0
event_2 = 900,0.5
```

O evento do step `k` é enfileirado imediatamente antes de `minisnn_step`.
Eventos do mesmo step são somados e limitados uma única vez. O parser ordena
por step e pelo índice `event_N`, com capacidade fixa de 256 eventos.

## 6. API pública

O fluxo básico é:

```c
MiniSNNRewardConfig reward = minisnn_default_reward_config();
reward.enabled = 1;
minisnn_set_reward_config(snn, &reward);
minisnn_queue_reward(snn, 1.0);
minisnn_step(snn);
```

A API também expõe reward pendente e aplicado, estatísticas globais,
elegibilidade e estatísticas por conexão, contagem de conexões elegíveis,
limpeza do sinal e `minisnn_reset_reward_learning`. O reset preserva pesos,
potenciais, step, configuração de plasticidade e estado homeostático; zera
eligibilidades, reward pendente/aplicado e estatísticas de reward.

## 7. Cenários e execução

Os demonstradores reproduzíveis são:

- `configs/reward_positive_demo.ini`;
- `configs/punishment_negative_demo.ini`;
- `configs/reward_delayed_demo.ini`;
- `configs/reward_mixed_demo.ini`.

Execute, por exemplo:

```powershell
mingw32-make scenario-reward-positive
mingw32-make scenario-punishment-negative
mingw32-make scenario-reward-delayed
mingw32-make scenario-reward-mixed
```

## 8. Outputs e métricas

Runs com reward ativo geram:

- `reward_metrics.csv`: série temporal agregada;
- `reward_events.csv`: componentes, valor bruto e valor aplicado;
- `reward_history.csv`: amostras globais e mudanças de peso;
- `eligibility_history.csv`: amostras determinísticas por conexão;
- `reward_connections.csv`: estado e acumulados finais por conexão;
- `reward_report.txt`: relatório textual em seções;
- `reward_report.html`: relatório local navegável;
- `reward_overview.png`: panorama em quatro painéis.

`metrics.csv`, `summary.txt`, `manifest.ini`, `weights_final.csv`, os relatórios
de métricas/pesos e a comparação de runs também identificam o modo e incorporam
os campos de reward quando disponíveis. Runs antigas sem esses arquivos seguem
analisáveis.

O histórico registra step 0, intervalos configurados, steps com reward e o
estado final. Quando há mais conexões elegíveis que
`record_connection_limit`, a miniSNN seleciona posições aproximadamente
uniformes ao longo da enumeração determinística das conexões elegíveis:

```text
posicao(k) = floor(k * (E - 1) / (L - 1))
```

`E` é a quantidade total elegível e `L` é o limite efetivo. Para `L = 1`, a
primeira conexão elegível é selecionada. Isso reduz o viés em direção aos
primeiros neurônios sem introduzir aleatoriedade. A amostra serve apenas para
históricos detalhados: métricas agregadas continuam considerando todas as
conexões elegíveis, `reward_connections.csv` mantém o snapshot completo e sua
coluna `sampled` identifica quais conexões participam do histórico.

## 9. Studio

O botão `RECOMPENSA` abre a configuração de R-STDP e o editor de eventos no
formato `300:1.0; 600:-1.0`. O modal `PLASTICIDADE` oferece `DIRECT STDP` e
`REWARD MODULATED STDP`. `GRAFICO RECOMPENSA` executa
`scripts/plot_reward.py`; `ABRIR RECOMPENSA` abre `reward_report.html` da
última pasta real da execução.

Combinações incompatíveis são recusadas. Cancelar um modal não deve modificar
a configuração. A validação visual e de interação do Studio continua sendo uma
etapa manual separada da suíte automatizada.

## 10. Testes e desempenho

```powershell
mingw32-make test-reward
mingw32-make test-reward-long
mingw32-make test-plot-reward
mingw32-make benchmark-c2
mingw32-make check-c2
```

Os testes cobrem sinais e fórmulas exatas, decaimento preguiçoso, clamps,
timing, reset, redes independentes, parser, runner, reprodutibilidade e execução
prolongada. O benchmark compara reward desligado, STDP direto e cinco variantes
de R-STDP. Resultados locais estão documentados em
[Benchmarks C2](BENCHMARKS_C2_RECOMPENSA.md).

## 11. Limitações científicas

R-STDP é uma aproximação simplificada e não garante aprendizado de tarefa.
Parâmetros ruins podem saturar ou silenciar pesos. Homeostase não garante
estabilidade universal. Punição é apenas reward negativo na fórmula. Não há
plasticidade INH, poda/criação de conexões, neuroevolução ou Worlds neste bloco.

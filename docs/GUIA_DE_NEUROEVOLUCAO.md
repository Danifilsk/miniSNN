# Guia de neuroevolucao

[Voltar ao indice](INDICE_DA_DOCUMENTACAO.md)

## Estado e escopo

O Bloco C3 adiciona um motor experimental de neuroevolucao para parametros de
uma rede miniSNN. A topologia e os delays sao capturados uma vez em um
**blueprint estrutural fixo**. Todos os individuos de um experimento possuem o
mesmo numero e a mesma ordem de genes.

O C3 nao implementa evolucao estrutural, poda, criacao de conexoes, NEAT,
paralelismo, GPU ou heranca lamarckiana. Esses temas ficam fora desta fase.

## Conceitos

- **Individuo:** um vetor de genes avaliado em uma rede nova.
- **Populacao:** conjunto de individuos de uma geracao.
- **Genoma:** valores herdaveis e seus limites.
- **Gene:** um peso inicial, magnitude INH ou parametro escalar.
- **Fenotipo:** rede reconstruida do blueprint com o genoma aplicado.
- **Fitness:** adequacao a funcao configurada, limitada ao intervalo `[0, 1]`.
- **Selecao:** escolha de pais por torneio sem reposicao.
- **Elitismo:** copia deterministica dos melhores para a proxima geracao.
- **Crossover:** combinacao uniforme de dois genomas.
- **Mutacao:** delta uniforme por gene, seguido de clamp.
- **Linhagem:** IDs, pais e operacao que originou cada individuo.
- **Diversidade genetica:** dispersao por gene e distancia media entre pares.
- **Checkpoint:** estado serializado na fronteira de uma geracao.

## Compilar e executar

```powershell
mingw32-make evolution-build
mingw32-make evolution-weight-demo
```

Execucao direta:

```powershell
.\build\evolution_runner.exe configs\evolution_weight_target_demo.ini
```

Retomada:

```powershell
.\build\evolution_runner.exe --resume results\evolution\nome_do_experimento
```

O runner e o formato INI sao a interface publica experimental do C3. O header
publico `minisnn.h` permanece compativel; `src/evolution.h` e o motor interno.

## Configuracao `[evolution]`

Campos principais:

```ini
[evolution]
enabled = true
experiment_name = meu_experimento
base_scenario = configs/random.ini
population_size = 20
generations = 30
elite_count = 2
selection = tournament
tournament_size = 3
crossover = uniform
crossover_rate = 0.80
mutation = uniform_delta
mutation_rate = 0.35
mutation_scale = 0.15
initialization = baseline_plus_mutation
initialization_scale = 0.25
evolution_seed = 12345
evaluation_replicates = 2
evaluation_seed_base = 50000
replicate_std_penalty = 0.10
checkpoint_interval_generations = 1
save_all_genomes = true
save_best_run = true
auto_unique_run = true
history_enabled = true
```

`experiment_name` aceita letras, numeros, `_` e `-`. `auto_unique_run` evita
sobrescrita. A seed evolutiva controla inicializacao, torneio, crossover e
mutacao. As seeds de avaliacao sao fixas por indice de replica.

## Configuracao `[genome]`

```ini
[genome]
evolve_exc_weights = true
exc_weight_min = 0.0
exc_weight_max = 500.0
evolve_inh_magnitudes = true
inh_magnitude_min = 0.0
inh_magnitude_max = 600.0
scalar_gene_0 = plasticity.a_plus,0.1,3.0,0.1
```

Ordem deterministica do genoma:

1. genes escalares em ordem de indice;
2. pesos EXC por `connection_id` crescente;
3. magnitudes INH por `connection_id` crescente.

Uma magnitude INH `m` e aplicada como peso `-m`. Assim, o sinal de Dale e
preservado. Bounds invalidos, genes duplicados ou modulos desativados sao
rejeitados.

Genes escalares suportados:

- `plasticity.a_plus`, `plasticity.a_minus`;
- `plasticity.tau_plus`, `plasticity.tau_minus`;
- `reward.learning_rate`, `reward.eligibility_tau`;
- `homeostasis.target_rate`, `homeostasis.rate_tau`;
- `homeostasis.threshold_eta`, `homeostasis.scaling_eta`;
- `homeostasis.inhibitory_gain_eta`.

## Configuracao `[fitness]`

Cada linha usa:

```text
metric,goal,target,scale,weight
```

Exemplo:

```ini
[fitness]
term_0 = neuron_spikes:2,target,3.0,1.5,1.0
term_1 = activity_total_spikes,minimize,20.0,20.0,0.2
```

Metas:

```text
target:   1 / (1 + abs(observed - target) / scale)
maximize: 1 / (1 + exp(-(observed - target) / scale))
minimize: 1 / (1 + exp( (observed - target) / scale))
```

O fitness da replica e a media ponderada dos termos. Para multiplas replicas:

```text
selection_fitness = clamp(mean - replicate_std_penalty * std, 0, 1)
```

Metricas incluem atividade total/EXC/INH, fracao ativa, primeiro/ultimo passo,
spikes de um neuronio, media/desvio de pesos finais, erro homeostatico e
mudancas de peso por reward. Uma metrica indisponivel invalida a avaliacao; nao
e substituida silenciosamente por zero.

## Blueprint e avaliacao

O runner reutiliza o construtor de topologia do runner de cenarios, captura os
pares, tipos, pesos e delays e reconstrui uma rede nova para cada replica. Nao
existe uma terceira implementacao de topologia no C3.

Ordem de uma avaliacao:

1. reconstruir a rede do blueprint;
2. aplicar genes escalares e pesos iniciais;
3. configurar STDP, R-STDP e homeostase;
4. executar o mesmo runtime temporal dos cenarios;
5. coletar metricas e fitness;
6. destruir a rede.

## Inicializacao e operadores

`baseline_plus_mutation` preserva o individuo zero exatamente no baseline e
perturba os demais. `uniform` amostra os bounds. O PRNG PCG32 e interno,
deterministico e serializavel; `rand()` e `srand()` nao sao usados.

O torneio amostra candidatos sem reposicao. Empates usam fitness medio, menor
desvio, geracao e ID para manter a ordem deterministica. Elites recebem novos
IDs e registram `elite_copy`. Crossover uniforme escolhe cada gene de um dos
pais. Mutacao soma um delta uniforme proporcional ao intervalo e registra
quantidade, magnitude e clamps.

## Plasticidade e heranca

STDP, R-STDP e homeostase podem operar durante a vida do fenotipo e influenciar
o fitness. Ao terminar a avaliacao, a rede e descartada. A descendencia recebe
o **genoma inicial selecionado**, nunca os pesos finais aprendidos.

```text
inheritance = darwinian
lamarckian_inheritance = disabled
```

`best_run/weights_initial.csv` registra o fenotipo herdavel reconstruido;
`best_run/weights_final.csv` registra o estado apos a vida. Os arquivos nao sao
intercambiaveis.

## Checkpoint e resume

`checkpoint.txt` e escrito atomicamente em fronteiras de geracao. Ele contem
configuracao do motor, estado PCG32, proximo ID, populacao, fitness, linhagem
necessaria e melhor global. `--resume` valida assinatura e genoma antes de
continuar. Uma retomada exata reproduz CSVs e melhor individuo da execucao
ininterrupta, exceto timestamps, caminhos e tempos de parede.

## Resultados

Cada experimento fica em `results/evolution/<actual_experiment_name>/`:

- `evolution_config_used.ini`, `base_scenario_used.ini`;
- `evolution_manifest.txt`, `evolution_report.txt`, `evolution_report.html`;
- `generations.csv`, `individuals.csv`, `replicates.csv`;
- `fitness_terms.csv`, `genomes.csv`, `lineage.csv`;
- `best_genome.csv`, `best_network_initial.csv`, `checkpoint.txt`;
- `evolution_overview.png` e `best_run/`.

`results/evolution/index.csv` e append-only. O historico local e
`results/evolution/history.html`. O arquivo `last_experiment.txt` so muda apos
sucesso e permite ao Studio localizar a ultima execucao.

## Graficos e relatorios

```powershell
mingw32-make plot-evolution RUN=results/evolution/meu_experimento
mingw32-make report-evolution RUN=results/evolution/meu_experimento
mingw32-make report-evolution-history
```

O PNG mostra fitness, diversidade, operadores e uma amostra deterministica de
genes. O HTML e local, autossuficiente, sem CDN e usa links relativos para os
CSVs. Python, pandas e matplotlib sao necessarios para o PNG.

## Studio

O botao `NEUROEVOLUCAO` abre uma janela separada. Ela permite carregar/salvar a
config, editar os controles principais, iniciar ou retomar e abrir pasta,
relatorio, grafico, melhor execucao e historico. O processo externo e
acompanhado por timer; o Studio nao inicia duas evolucoes simultaneas.

A compilacao automatica valida o codigo. Os cliques, seletores e mensagens
continuam marcados como validacao manual pendente no checklist.

## Demos

- `evolution_weight_target_demo.ini`: evolui pesos iniciais para uma contagem-alvo;
- `evolution_homeostasis_demo.ini`: evolui parametros homeostaticos;
- `evolution_plasticity_demo.ini`: evolui genoma inicial com STDP durante a vida.

## Testes

```powershell
mingw32-make test-evolution
mingw32-make test-evolution-runner
mingw32-make test-evolution-resume
mingw32-make test-evolution-long
mingw32-make test-plot-evolution
mingw32-make test-evolution-report
mingw32-make benchmark-c3
mingw32-make check-c3
```

## Honestidade cientifica

- Fitness mede somente o objetivo configurado.
- Melhor fitness nao significa inteligencia geral.
- A evolucao pode explorar atalhos da funcao de fitness.
- Resultados dependem de bounds, seeds e cenario.
- Convergencia e otimo global nao sao garantidos.
- Diversidade pode colapsar prematuramente.
- Topologia e delays nao evoluem no C3.
- Aprendizado ao longo da vida nao e herdado.
- Replicas reduzem parte da sensibilidade, mas nao eliminam variancia.
- A avaliacao e serial; paralelismo e trabalho futuro.

## Proximo marco

C4 permanece planejado para topologia adaptativa e evolucao estrutural. Nada
dessa fase e implementado ou simulado pelo C3.

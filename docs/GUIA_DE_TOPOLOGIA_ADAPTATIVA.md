# Guia de topologia adaptativa

Este guia descreve o Bloco C4 da miniSNN. O C4 mantém fixos o número, a
identidade e o tipo EXC/INH dos neurônios. Somente as arestas dirigidas, suas
magnitudes e seus delays podem variar.

## 1. C3 fixo e C4 estrutural

O modo `fixed_numeric` preserva o C3: a topologia do cenário é fixa e o genoma
contém pesos/magnitudes ou parâmetros escalares. O modo
`structural_connections` ativa um genoma esparso de conexões. A ausência de
`genome_mode` continua significando `fixed_numeric`.

```ini
[evolution]
genome_mode = structural_connections

[structure]
enabled = true
```

Os dois campos precisam ser compatíveis. Com C4 desligado não há estado
estrutural de lifetime alocado e a execução C3 segue o caminho histórico.

## 2. Blueprint e identidade

O `NeuronBlueprint` é imutável durante um experimento: quantidade, IDs, tipos e
parâmetros neurais não mudam. A topologia inicial do cenário é capturada uma
vez como `InitialConnectionBlueprint` e fornece o primeiro genoma estrutural.

A identidade de uma aresta é:

```c
connection_key = source * neuron_count + target;
```

O cálculo valida limites e overflow. Peso e delay são atributos, não identidade.
Os genes são ordenados por `connection_key`, sem duplicatas. A representação é
esparsa; conexões ausentes não ocupam uma matriz `N x N`.

## 3. Genoma estrutural

Cada `MiniSNNConnectionGene` contém `connection_key`, `source`, `target`,
`magnitude`, `delay` e `inherited_from`. A magnitude é não negativa. O peso
aplicado deriva do tipo fixo da origem:

- origem EXC: `+magnitude`;
- origem INH: `-magnitude`.

O sinal de Dale simplificado não pode ser invertido por mutação. Scalar genes
continuam na ordem declarada; connection genes usam ordem crescente de key.

## 4. Operações atômicas

`minisnn_validate_topology_patch` valida sem aplicar.
`minisnn_apply_topology_patch` aceita um lote de `ADD`, `REMOVE`, `REWIRE` e
`SET_DELAY`. O lote é montado sobre uma cópia, validado integralmente e só
então aplicado. Uma operação inválida preserva toda a rede anterior.

Após um patch aprovado, os índices incoming/outgoing e a enumeração são
reconstruídos uma vez. Conexões sobreviventes preservam peso, delay e estado
R-STDP por key; removidas descartam estado; novas começam com elegibilidade
zero. Traces e estado homeostático por neurônio são preservados.

Uma conexão adicionada no step `k` só participa de steps futuros. Uma removida
no fim de `k` já participou de `k`, mas não transmite novos spikes em `k+1`.

## 5. Configuração `[structure]`

Campos disponíveis:

```ini
[structure]
enabled = true
allow_add = true
allow_remove = true
allow_rewire = true
evolve_delays = true
add_rate = 0.15
remove_rate = 0.10
rewire_rate = 0.05
delay_mutation_rate = 0.10
max_structural_mutations_per_child = 2
min_connections = 4
max_connections = 64
allow_self_connections = false
allow_inh_to_inh = false
delay_min = 1
delay_max = 8
delay_mutation_max_delta = 2
new_exc_weight_min = 10.0
new_exc_weight_max = 200.0
new_inh_magnitude_min = 10.0
new_inh_magnitude_max = 200.0
complexity_penalty = 0.02
preserve_required_reachability = false
required_input_neurons = 0,1
required_output_neurons = 8
```

Taxas são probabilidades independentes em `[0,1]`. Min/max precisam caber no
conjunto de pares legais. Delays precisam caber no limite real do Core. Com
reachability ligada, cada saída obrigatória deve ser alcançável por ao menos
uma entrada em um grafo dirigido. Reachability estrutural não garante spikes.

## 6. Pares legais

`structure_is_legal_pair` centraliza endpoints válidos, duplicatas,
auto-conexões e INH->INH. Crossover, mutação e plasticidade de lifetime usam a
mesma regra. Um cenário pode autorizar auto-conexão explicitamente no patch.

## 7. Crossover e mutação

Genes correspondentes são alinhados por key e magnitude/delay vêm juntos de
um dos pais. Com fitness diferente, genes disjuntos vêm apenas do pai mais
apto; em empate, cada disjunto tem chance de 50%. Se tentativas limitadas não
gerarem filho válido, o pai mais apto é clonado e o fallback é registrado.

A ordem reprodutiva C4 é:

1. crossover ou clone;
2. mutações estruturais add/remove/rewire/delay;
3. canonicalização e validação;
4. mutações numéricas escalares e de magnitude;
5. validação final.

Remoções respeitam min e reachability. Add escolhe pares ausentes com PRNG
privado, tentativas limitadas e fallback determinístico. Delay usa delta inteiro
não zero e clamp. Operações sem candidato são registradas como skipped.

## 8. Complexidade e diversidade

O fitness robusto permanece o do C3. Depois é aplicada:

```text
C = (connections - min_connections) / (max_connections - min_connections)
fitness_selection = clamp(robust_fitness - complexity_penalty * C, 0, 1)
```

Quando os limites são iguais, `C=0`. O runner registra `behavior_fitness`,
`robust_fitness`, complexidade, valor da penalidade e fitness de seleção.

No `evolution_structure_target_demo`, a fitness comportamental permaneceu
equivalente; a queda observada na fitness de selecao vem da menor penalidade de
complexidade, pois a rede foi reduzida de quatro para duas conexoes. O demo nao
evidencia melhora comportamental.

Diversidade topológica usa distância de Jaccard sobre connection keys:

```text
D(A,B) = 1 - |A interseção B| / |A união B|
```

Peso não participa dessa distância.

## 9. Plasticidade estrutural durante a vida

É opcional e configurada no cenário-base:

```ini
[structural_plasticity]
enabled = true
maintenance_interval_steps = 100
grace_period_steps = 500
pruning_enabled = true
prune_weight_threshold = 0.50
prune_activity_threshold = 0.001
max_prunes_per_interval = 1
growth_enabled = true
growth_candidate_count = 16
growth_score_threshold = 0.010
max_growth_per_interval = 1
growth_seed = 9001
new_exc_weight = 5.0
new_inh_magnitude = 5.0
new_delay = 1
min_connections = 4
max_connections = 64
record_history = true
record_interval_steps = 100
```

O módulo mantém rate traces próprios, sem depender de homeostase. Para uma
conexão existente, a heurística de uso é `abs(weight) * rate_pre * rate_post`.
A poda exige peso e uso abaixo dos thresholds e idade acima do grace period.
O crescimento usa coatividade `rate_pre * rate_post`, considera no máximo o
número configurado de candidatos e ordena por score e key.

Poda e crescimento de um intervalo formam um único patch: podas virtuais vêm
primeiro, crescimentos são escolhidos na estrutura resultante e os índices são
reconstruídos uma vez.

## 10. Ordem temporal

Quando todos os módulos estão ativos:

1. correntes agendadas e integração LIF;
2. spikes e transmissão na topologia atual;
3. STDP direto ou atualização de elegibilidade;
4. reward pendente;
5. traces, threshold, scaling e ganho homeostático;
6. traces e manutenção estrutural;
7. registro dos históricos.

Logo, reward do step atua sobre a elegibilidade anterior ao patch estrutural.
Conexões novas não recebem reward, transmissão ou STDP retroativos.

## 11. Estado e resets

O estado opcional por conexão registra key, nascimento, última manutenção,
maior peso absoluto, activity score, candidaturas à poda e origem de growth.
Ele é reconstruído por key.

- `MINISNN_STRUCTURAL_RESET_STATE`: preserva a topologia e zera estado/traces;
- `MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY`: restaura topologia, pesos e
  delays capturados e zera o estado estrutural.

## 12. Herança darwiniana

O genoma inicial do pai é o material reprodutivo. STDP, R-STDP, homeostase e
plasticidade estrutural alteram apenas o fenótipo avaliado. Pesos finais,
eligibilities, traces, thresholds e topologia final de lifetime não retornam ao
genoma. Herança lamarckiana permanece desativada.

`best_topology.csv` e `best_topology_initial.csv` representam o genoma
herdável. `best_topology_lifetime_final.csv` representa o fenótipo observado no
fim da avaliação e pode ser diferente.

## 13. Checkpoint e resume

O checkpoint C3 textual continua em `checkpoint.txt`. C4 adiciona
`checkpoint_structure.txt`, com assinatura da configuração e dos neurônios,
população estrutural completa, magnitudes, delays, linhagem, melhor global,
IDs e estado do PRNG. O arquivo C3 é escrito por último como marcador de
checkpoint completo. Resume valida keys, bounds, assinaturas e versão antes de
continuar, sem duplicar gerações.

## 14. Outputs

Cenários com plasticidade estrutural geram:

- `topology_initial.csv` e `topology_final.csv`;
- `structural_plasticity_metrics.csv`;
- `structural_plasticity_events.csv`;
- `topology_history.csv`;
- `topology_report.txt` e `topology_report.html`;
- `topology_overview.png` após `scripts/plot_topology.py`.

Evoluções C4 geram `structures.csv`, `structural_events.csv`, os três arquivos
de melhor topologia, métricas estendidas, checkpoint sidecar, relatório e PNG
evolutivos. O histórico aceita execuções C3 antigas com `NA`.

No Studio, `ABRIR EVENTOS ESTRUTURAIS` gera e abre
`structural_events_report.html`; `ABRIR MELHOR TOPOLOGIA` gera e abre
`best_topology_report.html`. `structural_events.csv` e `best_topology.csv`
permanecem como dados cientificos brutos. Sem Python, somente um HTML anterior
existente pode ser aberto com aviso de desatualizacao.

## 15. Execução

```powershell
mingw32-make evolution-structure-demo
mingw32-make structural-pruning-demo
mingw32-make structural-growth-demo
mingw32-make evolution-structure-learning-demo
mingw32-make test-structure
mingw32-make test-structural-plasticity
mingw32-make test-structure-resume
mingw32-make test-structure-long
mingw32-make benchmark-c4
mingw32-make check-c4
```

No Studio, abra `NEUROEVOLUCAO` e depois `TOPOLOGIA ADAPTATIVA`. O modal edita
o genoma estrutural e a plasticidade do cenário-base. Aplicar valida; cancelar
preserva os valores anteriores. Os botões científicos abrem relatório, PNG,
eventos e melhor topologia.

## 16. Limitações e honestidade científica

- coatividade não prova causalidade;
- poda e crescimento são heurísticas de engenharia;
- crescimento não garante melhora de fitness;
- evolução pode explorar falhas da função de fitness;
- menor complexidade não é sempre melhor;
- crossover pode destruir estruturas úteis;
- reachability estrutural não garante atividade;
- a topologia final pode depender fortemente da seed;
- neurônios não são criados nem removidos;
- NEAT, speciation e innovation numbers globais não foram implementados;
- lifetime rewiring não é herdado.

O C4 não representa neurogênese, desenvolvimento cerebral biologicamente fiel
ou evolução natural completa.

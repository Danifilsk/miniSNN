# Guia do miniSNN Studio

## RECOMPENSA

`RECOMPENSA` abre o modal de R-STDP sem sobrecarregar o painel principal. Ele
configura ON/OFF, `R-STDP`, learning rate, tau/bounds de elegibilidade,
bounds/clipping de reward, histórico, intervalo, limite de conexões e eventos
como `300:1.0; 600:-1.0`. O modal `PLASTICIDADE` seleciona `DIRECT STDP` ou
`REWARD MODULATED STDP`; combinações sem efeito são recusadas.

`GRAFICO RECOMPENSA` usa `scripts/plot_reward.py` na última pasta real da run,
gera `reward_overview.png` e atualiza `reward_report.html`. `ABRIR RECOMPENSA`
gera o HTML sob demanda e o abre no navegador. Uma run sem reward mostra:
"Esta execucao nao possui dados de recompensa. O aprendizado por recompensa
pode estar desligado."

Os fluxos reutilizam a detecção configurável de Python. A compilação valida a
integração, mas abertura, cancelamento, layout e os 34 itens de interação do C2
continuam com **VALIDAÇÃO MANUAL PENDENTE**.

[Voltar ao índice da documentação](INDICE_DA_DOCUMENTACAO.md)

## 1. O que e o miniSNN Studio

O miniSNN Studio e a primeira interface grafica da miniSNN para Windows. Ele
usa o mesmo motor de cenarios em `app/`, sem duplicar a simulacao e sem alterar
o nucleo em `src/`.

Com ele voce pode criar, carregar, salvar e executar cenarios sem editar C.

## 2. Como compilar

Na raiz do projeto:

```powershell
mingw32-make studio
```

Esse comando compila e abre a interface.

## 3. Como abrir o executavel diretamente

Depois de compilar:

```powershell
build\minisnn_studio.exe
```

O Studio ajusta o diretorio de trabalho para a raiz do projeto quando aberto a
partir de `build/`.

## 4. Como criar um cenario novo

Clique em `NOVO PADRAO`. Os campos recebem uma configuração pequena e segura:
rede `random_balanced`, 20 neuronios, 20% inibitorios, pesos EXC/INH e entrada
nos primeiros neuronios.

Altere os campos desejados e rode a simulacao.

## 5. Como carregar um .ini existente

Clique em `CARREGAR CENARIO` e escolha um arquivo em `configs/`. O Studio usa o
mesmo parser do runner de terminal, entao chaves desconhecidas, duplicadas ou
valores invalidos sao rejeitados.

Topologias disponiveis no campo `Topologia`:

```text
chain
ring
all_to_all
random
random_balanced
small_world
feedforward
```

## 5.1. Como usar `OPCOES`

O botao `OPCOES`, ao lado da topologia, abre uma janela com parametros menos
usados:

```text
Permitir auto-conexao
Permitir INH -> INH
Densidade de conexao
Seed
Delay
Delay maximo
Small-world: vizinhos locais
Small-world: probabilidade de reconexao
Feedforward: numero de camadas
```

Campos que nao se aplicam a topologia atual ficam desabilitados. Regras:

- `Densidade de conexao`: usada em `random`, `random_balanced` e `feedforward`.
- `Seed`: usada em `random`, `random_balanced`, `small_world` e `feedforward`.
- `Small-world`: usado somente em `small_world`.
- `Feedforward`: usado somente em `feedforward`.
- `Permitir auto-conexao`: habilitado em `all_to_all`, `random`,
  `random_balanced` e `small_world`; desabilitado em `chain`, `ring` e
  `feedforward`.
- `Permitir INH -> INH`: so faz diferenca quando ha neuronios inibitorios.

Quando um checkbox fica desabilitado, o valor e mantido no cenario, mas a
interface mostra o texto em cinza para indicar que ele nao se aplica a
topologia ou configuracao atual.

Clique em `APLICAR` para validar e atualizar o cenario em tela. Clique em
`CANCELAR` para fechar sem alterar.

## 6. Como salvar um cenario

Clique em `SALVAR CENARIO`. O Studio valida todos os campos e grava um `.ini`
compativel com `app/minisnn_runner.c`.

Use nomes de execucao diferentes quando quiser preservar resultados antigos.

## 7. Como rodar uma simulacao

Clique em `RODAR SIMULACAO`. O Studio cria:

```text
results/scenarios/<actual_run_name>/
```

O Studio sempre evita sobrescrita. Se `run_name` ja existir, a nova execucao
recebe uma pasta com timestamp, e o painel mostra a pasta real usada.

E grava:

```text
config_used.ini
summary.txt
population.csv
raster.csv
neuron_<id>.csv
run_manifest.txt
```

Com diagnóstico `basic` ou `full`, o runner também cria `metrics.csv`. Relatório
e gráficos diagnósticos são produzidos por `GERAR DIAGNOSTICO`.

O painel da direita mostra conexoes, spikes totais, spikes EXC/INH e timesteps
ativos.

O campo `Neuronio detalhado` define qual neuronio tera um CSV individual salvo
como:

```text
results/scenarios/<actual_run_name>/neuron_<id>.csv
```

Esse CSV contem:

```text
tempo,V,spike,corrente_externa,corrente_sinaptica
```

Use `CSV NEURONIO` para abrir esse arquivo depois da simulação. Use
`GRAFICO NEURONIO` para gerar:

```text
neuron_<id>_detail.png
```

O grafico mostra potencial de membrana, spikes, corrente externa e corrente
sinaptica. Depois de gerar, use `ABRIR GRAFICO` para abrir o PNG no visualizador
padrao do Windows.

## 8. Como gerar graficos

Depois de uma simulação, clique em `GERAR GRAFICOS`.

O Studio procura Python automaticamente. A busca considera:

```text
1. Variavel de ambiente MINISNN_PYTHON.
2. Instalacoes em %LOCALAPPDATA%\Programs\Python\*\python.exe.
3. Instalacoes em %LOCALAPPDATA%\Python\*\python.exe.
4. Python do MSYS2.
5. py.exe no PATH.
6. python.exe no PATH.
```

Quando encontra um interpretador, o Studio testa primeiro:

```text
import pandas, matplotlib
```

So depois disso ele executa o script com caminhos entre aspas, sem abrir
terminal visivel.

Os arquivos gerados sao:

```text
population_activity.png
mean_state.png
raster.png
```

Para gerar manualmente o grafico de um unico neuronio:

```powershell
python scripts/plot_neuron.py results/scenarios/<actual_run_name> <neuron_id>
```

Se o Studio nao encontrar Python valido, ele mostra uma mensagem curta pedindo
uma instalacao compativel com `pandas` e `matplotlib`.

Evite usar Python de jogos, caches, runtimes internos ou pastas como Steam,
WARNO e codex-runtimes. Quando o Studio estiver configurado, o terminal nao e
necessario para gerar graficos.

## 9. Como abrir resultados e historico

Use:

- `ABRIR RESULTADOS`: abre a pasta `results/`.
- `ABRIR ULTIMA`: abre a pasta real da ultima execucao rodada.
- `ABRIR HISTORICO`: gera e abre `results/scenarios/history.html`.

Se nenhuma simulacao foi rodada ainda, `ABRIR ULTIMA` mostra uma mensagem
amigavel. `ABRIR HISTORICO` permanece habilitado: cria o cabeçalho de
`index.csv` quando necessário, executa `scripts/generate_history_report.py` e
abre o HTML no navegador padrão. O CSV continua append-only e acessível pelo
próprio relatório e por `ABRIR RESULTADOS`.

O HTML é local, escuro e autossuficiente. Mostra cards, runs mais recentes
primeiro, busca, filtros e links para artefatos existentes. Runs cujas pastas
foram removidas permanecem visíveis sem links. Se Python não estiver disponível
mas houver um `history.html` anterior, o Studio avisa que pode estar
desatualizado e o abre; sem Python e sem HTML, mostra um erro claro e não abre o
CSV silenciosamente.

## 10. Como comparar execucoes

Clique em `COMPARAR EXECUCOES` para selecionar duas pastas existentes em:

```text
results/scenarios/
```

O Studio chama `scripts/compare_runs.py` usando o Python detectado
automaticamente. A comparacao e salva em:

```text
results/comparisons/<comparison_name>/
```

Se o nome de comparacao ja existir, o script cria uma pasta unica por padrao e
registra `results/comparisons/index.csv`.

Arquivos gerados:

```text
comparison_summary.csv
comparison_report.txt
comparison_metrics.png
comparison_activity_overlay.png
```

Depois de gerar, clique em `ABRIR COMPARACAO` para abrir a pasta no Explorer.
Se o usuario cancelar a selecao de pasta, o Studio apenas cancela a operacao.

## 11. Como evitar sobrescrever uma execucao anterior

Os resultados são gravados em modo de escrita. No runner de terminal, uma pasta
pode ser reutilizada quando `auto_unique_run = false`. O Studio força nome único
e preserva a pasta anterior.

Para comparar testes:

1. Mude `Nome da execucao`.
2. Rode a simulacao.
3. Gere os graficos.
4. Compare as pastas em `results/scenarios/`.

## 12. Diagnostico

O seletor `DIAG` oferece `OFF`, `BASIC` e `FULL`. Novos cenarios usam `BASIC`.
Depois de uma execucao, `GERAR DIAGNOSTICO` chama `scripts/analyze_run.py` com o
Python detectado. `ABRIR METRICAS` abre `metrics_report.html` no navegador
padrão. Se o HTML ainda não existir e `metrics.csv` estiver disponível, o
Studio chama `scripts/generate_run_reports.py` para criá-lo na última pasta real
(`actual_run_name`). `ABRIR DIAGNOSTICO` continua abrindo
`diagnostics_overview.png`. Em `OFF`, nenhuma análise nova é executada
automaticamente, mas uma run antiga que já tenha `metrics.csv` pode receber o
HTML sob demanda. O relatório funciona localmente, sem internet, e contém links
para o CSV bruto e demais arquivos da execução.

## 13. Plasticidade

`PLASTICIDADE` abre um modal próprio. Ele controla STDP ON/OFF, regra
`stdp_pair_trace`, A+/A-, taus, incremento dos traces, limites e registro. O
botão `CANCELAR` não aplica alterações; `APLICAR` valida o cenário completo.

Depois de uma run ativa, `GRAFICO STDP` executa `scripts/plot_plasticity.py` na
última pasta real e também atualiza `weights_report.html`. `ABRIR PESOS` abre
esse HTML no navegador padrão; se necessário, ele é gerado sob demanda a partir
de `weights_final.csv`. `ABRIR STDP` continua abrindo
`plasticity_overview.png`. O CSV completo permanece acessível pela pasta e pelos
links do relatório. Ausência dos arquivos produz mensagem controlada; uma run
com STDP OFF normalmente não possui relatório de pesos. Configs antigas e novos
cenários comuns carregam STDP OFF.

## 14. O que ainda nao existe no Studio

O Studio não implementa peixe, mundo, recompensa, punição, neuroevolução,
plasticidade inibitória ou topologia adaptativa. Ele expõe o STDP excitatório e
a homeostase simplificada já existentes no motor de cenários.

## HOMEOSTASE

`HOMEOSTASE` abre um modal separado com ativação global, mecanismo intrínseco,
taxa-alvo, tau, intervalo, clamps de threshold, scaling EXC, ganho INH e opções
de histórico. Cancelar não altera a configuração. `GRAFICO HOMEOSTASE` gera
`homeostasis_overview.png`; `ABRIR HOMEOSTASE` abre
`homeostasis_report.html`. Uma run sem dados mostra que a homeostase pode estar
desligada. A validação manual desses controles permanece no checklist do Studio.

## NEUROEVOLUCAO

`NEUROEVOLUCAO` abre uma janela separada para não sobrecarregar o painel de
cenários. Ela expõe config, cenário-base, nome, população, gerações, elites,
torneio, taxas de crossover/mutação, réplicas, seeds, genes de peso e campos
multilinha para genes escalares e termos de fitness.

`RODAR EVOLUCAO` salva e inicia `build/evolution_runner.exe` com
`CreateProcessA`. `RETOMAR EVOLUCAO` seleciona uma pasta com checkpoint. O
processo roda fora da thread principal; um timer atualiza o status e impede duas
evoluções simultâneas.

Após sucesso, `ABRIR EVOLUCAO`, `ABRIR RELATORIO`, `ABRIR GRAFICO` e
`ABRIR MELHOR` usam `results/evolution/last_experiment.txt`.
`HISTORICO EVOLUTIVO` gera/abre `results/evolution/history.html`. Python com
pandas e matplotlib é necessário para o PNG; sem Python, um HTML anterior só é
aberto com aviso. A compilação foi automatizada; os cliques permanecem com
validação manual pendente.

## Topologia adaptativa

Dentro de `NEUROEVOLUCAO`, o botão `TOPOLOGIA ADAPTATIVA` abre os campos de
`[structure]` e da plasticidade estrutural do cenário-base. `ABRIR TOPOLOGIA`
abre o relatório HTML, `GRAFICO TOPOLOGIA` abre o panorama PNG,
`ABRIR EVENTOS ESTRUTURAIS` gera e abre `structural_events_report.html` e
`ABRIR MELHOR TOPOLOGIA` gera e abre `best_topology_report.html`. Os CSVs
`structural_events.csv` e `best_topology.csv` permanecem como dados científicos
brutos. Sem Python, o Studio abre apenas um HTML anterior existente e avisa que
ele pode estar desatualizado. Aplicar valida
limites, delays e reachability; cancelar preserva os valores. A compilação foi
validada automaticamente, mas os cliques e caminhos com espaços permanecem em
validação manual.

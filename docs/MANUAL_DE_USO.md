# Manual de uso da miniSNN

[Voltar ao índice da documentação](INDICE_DA_DOCUMENTACAO.md)

## O que e a miniSNN

A miniSNN e uma biblioteca e simulador em C para redes neurais pulsadas
simplificadas.

Ela usa neuronios LIF, do ingles Leaky Integrate-and-Fire. O projeto tambem
suporta conexoes excitadoras e inibidoras, delays sinapticos configuraveis e
corrente sinaptica com decaimento.

A miniSNN e um projeto exploratorio e educacional. Ela ajuda a testar ideias,
observar regimes de atividade e estudar dinamicas simples de redes pulsadas.
Ela nao e uma simulacao biologicamente completa do cerebro.

## Requisitos no Windows

Use os comandos a partir da raiz do projeto, onde ficam `Makefile`,
`README.md`, `src/` e `include/`.

Requisitos principais:

- GCC via MSYS2/MinGW.
- `mingw32-make`.
- Python para scripts e graficos.
- Bibliotecas Python `pandas` e `matplotlib`.

Verifique o ambiente:

```powershell
gcc --version
mingw32-make --version
python --version
```

Se as bibliotecas Python nao estiverem instaladas:

```powershell
python -m pip install pandas matplotlib
```

No ambiente Windows atual do projeto, o comando disponivel e:

```powershell
mingw32-make
```

Use `make` somente se esse comando existir no seu PATH.

## Estrutura de pastas

```text
include/                API publica
src/                    nucleo interno
tests/                  testes automatizados
examples/api/           exemplos que usam apenas minisnn.h
examples/internal/      demo usando estruturas internas
experiments/            experimentos cientificos
configs/                cenarios configuraveis em formato INI
scripts/                scripts Python ativos
scripts/legacy/         scripts antigos preservados
results/api/            CSVs dos exemplos publicos
results/internal_demo/  saida do demo interno
results/experiments/    resultados cientificos
results/scenarios/      resultados locais dos cenarios
results/comparisons/    comparacoes locais de execucoes
results/archive/        resultados antigos preservados
build/                  executaveis gerados localmente
docs/                   manuais do projeto
```

Atencao:

- Nao edite `build/`. Essa pasta contem executaveis recriaveis.
- Nao use `results/archive/` como saida de experimentos novos. Ela e apenas
  historica.

## Primeiro fluxo seguro

Se voce acabou de abrir o projeto e quer saber se tudo esta funcionando, rode:

```powershell
mingw32-make test
mingw32-make api-examples
mingw32-make ei-balance
```

O que cada comando faz:

- `mingw32-make test` compila e executa os testes principais.
- `mingw32-make api-examples` executa exemplos que usam somente a API publica.
- `mingw32-make ei-balance` roda um experimento pequeno que compara excitacao
  isolada com excitacao mais inibicao.

Para limpar arquivos recriaveis:

```powershell
mingw32-make clean
```

Esse comando remove executaveis de `build/` e resultados recriaveis de:

```text
results/api/
results/internal_demo/
```

Ele nao apaga:

```text
results/archive/
results/experiments/
```

Resultados locais de cenarios e comparacoes tambem sao recriaveis, mas podem
conter analises importantes. Consulte `docs/ORGANIZACAO_DE_RESULTADOS.md`
antes de limpar manualmente.

## Todos os comandos disponiveis

| Comando | O que faz | Resultado esperado |
| --- | --- | --- |
| `mingw32-make` | Executa o alvo padrao | Equivale a `mingw32-make test` |
| `mingw32-make help` | Mostra os comandos disponiveis | Lista curta de alvos |
| `mingw32-make test` | Executa todos os testes | API, nucleo e LIF passam |
| `mingw32-make test-api` | Testa a API publica | `MiniSNN public API validation OK` |
| `mingw32-make test-core` | Testa nucleo, topologias, estimulos e recorders | Validacao completa do nucleo |
| `mingw32-make test-lif` | Testa o LIF basico | Total de spikes impresso |
| `mingw32-make test-scenario` | Testa o parser de cenarios | `Scenario configuration validation OK` |
| `mingw32-make test-runner` | Testa o executor de cenarios | `Scenario runner validation OK` |
| `mingw32-make test-compare-runs` | Testa comparacao de execucoes | `Run comparison validation OK` |
| `mingw32-make test-diagnostics` | Testa diagnosticos basic/full | `Run diagnostics validation OK` |
| `mingw32-make test-docs` | Valida links e referências documentais | `Documentation validation OK` |
| `mingw32-make api-examples` | Executa os tres exemplos publicos | CSVs em `results/api/` |
| `mingw32-make api-single` | Exemplo de um neuronio | `api_single_neuron.csv` |
| `mingw32-make api-chain` | Exemplo de cadeia | `api_chain_spikes.csv` |
| `mingw32-make api-exc-inh` | Exemplo EXC vs EXC/INH pela API | CSVs do alvo em `results/api/` |
| `mingw32-make demo` | Executa demo interno | CSVs em `results/internal_demo/` |
| `mingw32-make scenario` | Executa o cenario padrao | Resultados em `results/scenarios/<actual_run_name>/` |
| `mingw32-make scenario-random` | Executa `configs/random.ini` | Cenario aleatorio simples |
| `mingw32-make scenario-small-world` | Executa `configs/small_world.ini` | Cenario small-world |
| `mingw32-make scenario-feedforward` | Executa `configs/feedforward.ini` | Cenario em camadas |
| `mingw32-make studio-build` | Compila o miniSNN Studio | `build/minisnn_studio.exe` |
| `mingw32-make studio` | Compila e abre o miniSNN Studio | Interface grafica para cenarios |
| `mingw32-make ei-balance` | Experimento EXC vs EXC/INH | Resultados em `results/experiments/ei_balance/` |
| `mingw32-make inhibition-fine` | Varredura fina de inibicao | CSV em `results/experiments/inhibition/` |
| `mingw32-make inh-to-inh` | Compara com e sem INH -> INH | CSVs em `results/experiments/inh_to_inh/` |
| `mingw32-make sparse-ei` | Analise EXC/INH em redes esparsas | CSVs em `results/experiments/sparse_ei/` |
| `mingw32-make clean` | Limpa arquivos recriaveis | Remove build e saidas locais recriaveis |

## API publica versus nucleo interno

Para criar uma aplicacao nova, agente ou cerebro de peixe, prefira a API
publica em:

```text
include/minisnn.h
```

Fluxo recomendado:

1. Crie uma `MiniSNNConfig`.
2. Ajuste `neuron_count` e parametros desejados.
3. Crie a rede com `minisnn_create_with_config`.
4. Aplique entradas com `minisnn_set_input` ou `minisnn_add_input`.
5. Avance a simulacao com `minisnn_step`.
6. Leia estado com `minisnn_get_spike`, `minisnn_get_voltage` e
   `minisnn_get_synaptic_current`.

Para criar experimentos cientificos atuais, use os arquivos em `experiments/`.
Eles usam `src/network.h`, `src/recorder.h` e outras estruturas internas para
ter mais controle sobre a simulacao.

Evite misturar os dois estilos no mesmo programa sem necessidade. Para codigo
novo de usuario, comece pela API publica.

## Cenarios configuraveis

Para testes normais de topologia e parametros principais, prefira editar um
arquivo `.ini` em:

```text
configs/
```

O runner fica em:

```text
app/minisnn_runner.c
```

Ele executa os cenarios usando a API publica e grava resultados locais em:

```text
results/scenarios/
```

Exemplo:

```powershell
mingw32-make scenario SCENARIO=configs/random_balanced.ini
```

Use cenarios para mudar topologia, pesos, seed, numero de neuronios, duracao,
entrada externa e opcoes de conectividade. As topologias disponiveis sao:
`chain`, `ring`, `all_to_all`, `random`, `random_balanced`, `small_world` e
`feedforward`. Nao altere `src/` para testes normais de parametros.

## miniSNN Studio

O uso normal de cenarios tambem pode ser feito pelo Studio:

```powershell
mingw32-make studio
```

A interface permite criar um cenario novo, carregar um `.ini`, salvar ajustes,
executar a simulacao, gerar graficos e abrir a pasta de resultados. A pasta
`configs/` continua sendo o caminho recomendado para reproducao e uso avancado,
pois os arquivos `.ini` deixam os parametros explicitos.

O campo `Neuronio detalhado` escolhe o neuronio salvo em
`results/scenarios/<actual_run_name>/neuron_<id>.csv`. Depois de rodar a simulacao, o
Studio pode abrir esse CSV e gerar `neuron_<id>_detail.png`, com potencial de
membrana, spikes, corrente externa e corrente sinaptica. Pelo terminal:

```powershell
python scripts/plot_neuron.py results/scenarios/<actual_run_name> <neuron_id>
```

Para comparar duas execucoes pelo terminal:

```powershell
python scripts/compare_runs.py results/scenarios/random_demo results/scenarios/small_world_demo --out-name random_vs_small_world
```

Os resultados ficam em:

```text
results/comparisons/<comparison_name>/
```

O Studio tambem possui `COMPARAR EXECUCOES` e `ABRIR COMPARACAO` para gerar e
abrir essa pasta sem digitar comandos.

O Studio evita sobrescrita automaticamente. Ao rodar duas vezes o mesmo
`run_name`, a segunda saida recebe timestamp. Use `ABRIR RESULTADOS` para abrir
`results/`, `ABRIR ULTIMA` para abrir a pasta real mais recente e `ABRIR
HISTORICO` para abrir `results/scenarios/index.csv`.

O botao `OPCOES`, ao lado da topologia, edita configuracoes menos frequentes:
auto-conexao, conexoes `INH -> INH`, densidade, seed, delays, parametros
`small_world` e numero de camadas `feedforward`. O Studio desabilita campos que
nao se aplicam a topologia selecionada. Auto-conexoes reais podem ser ativadas
em `all_to_all`, `random`, `random_balanced` e `small_world`; em `chain`,
`ring` e `feedforward`, a opcao fica desabilitada.

Na API publica, as chamadas antigas `minisnn_connect()` e
`minisnn_connect_delayed()` continuam rejeitando auto-conexoes. Use
`minisnn_connect_delayed_ex(..., allow_self_connection)` quando o experimento
precisar permitir explicitamente `source == target`.

Para gerar graficos, o Studio tenta localizar Python automaticamente nesta
ordem: variavel `MINISNN_PYTHON`, instalacoes em
`%LOCALAPPDATA%\Programs\Python\`, `%LOCALAPPDATA%\Python\`, MSYS2, `py.exe`
no PATH e `python.exe` no PATH. Antes de usar um interpretador, o Studio testa
se `pandas` e `matplotlib` estao disponiveis. Se nada valido for encontrado,
ele mostra uma mensagem curta pedindo uma instalacao compativel. O terminal nao
e necessario quando o Studio estiver configurado.

O Studio ainda mostra apenas recursos ja implementados no motor de cenarios:
topologias configuraveis, pesos EXC/INH, entrada externa constante, parametros
LIF e CSVs/graficos. Ele nao adiciona plasticidade, recompensa, mundo externo
ou topologia adaptativa.

## Resultados e sobrescrita

Cada execucao gera `run_manifest.txt`. Com diagnostico `basic` ou `full`, o
runner tambem gera `metrics.csv`; `scripts/analyze_run.py` completa as metricas,
o relatorio e os graficos. O guia detalhado esta em
`docs/GUIA_DE_DIAGNOSTICO.md`.

Os programas abrem os CSVs em modo `"w"`. Ao executar novamente o mesmo
experimento, os arquivos de resultado daquele experimento sao substituidos.

Receita segura antes de testar parametros novos:

1. Execute o experimento padrao.
2. Copie os CSVs ou PNGs importantes para uma pasta com nome descritivo dentro
   de `results/experiments/`.
3. Modifique um parametro.
4. Execute novamente.
5. Compare os resultados.

Nao salve resultados novos em `results/archive/`. Essa pasta e apenas
historica.

## Problemas comuns

### `mingw32-make` nao e reconhecido

O MSYS2/MinGW pode nao estar instalado ou pode nao estar no PATH. Confirme:

```powershell
mingw32-make --version
```

Se falhar, instale ou abra um terminal com o ambiente MinGW correto.

### `gcc` nao e reconhecido

Confirme:

```powershell
gcc --version
```

Se falhar, instale GCC via MSYS2/MinGW ou ajuste o PATH.

### Python nao encontra `pandas` ou `matplotlib`

Instale as bibliotecas no Python usado pelos scripts:

```powershell
python -m pip install pandas matplotlib
```

O Studio valida automaticamente o interpretador antes de gerar graficos. Python
de jogos, caches ou runtimes internos nao e recomendado.

### O script Python diz que um CSV esta ausente

Rode primeiro o experimento que gera os CSVs esperados. Por exemplo:

```powershell
mingw32-make ei-balance
```

Depois rode o script correspondente.

### O experimento sobrescreveu um CSV anterior

Isso e esperado: os arquivos sao abertos em modo `"w"`. Antes de alterar
parametros, copie os resultados-base para uma pasta de comparacao.

### `results/api` ou `results/internal_demo` esta vazio depois de `clean`

Isso e esperado. `mingw32-make clean` remove resultados recriaveis dessas
pastas. Rode novamente:

```powershell
mingw32-make api-examples
mingw32-make demo
```

### O grafico abre e o terminal parece travado

Os scripts Python usam `plt.show()`. Feche a janela do grafico para o terminal
encerrar.

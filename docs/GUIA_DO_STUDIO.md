# Guia do miniSNN Studio

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

Clique em `Novo padrao`. Os campos recebem uma configuracao pequena e segura:
rede `random_balanced`, 20 neuronios, 20% inibitorios, pesos EXC/INH e entrada
nos primeiros neuronios.

Altere os campos desejados e rode a simulacao.

## 5. Como carregar um .ini existente

Clique em `Carregar cenario` e escolha um arquivo em `configs/`. O Studio usa o
mesmo parser do runner de terminal, entao chaves desconhecidas, duplicadas ou
valores invalidos sao rejeitados.

## 6. Como salvar um cenario

Clique em `Salvar cenario`. O Studio valida todos os campos e grava um `.ini`
compativel com `app/minisnn_runner.c`.

Use nomes de execucao diferentes quando quiser preservar resultados antigos.

## 7. Como rodar uma simulacao

Clique em `Rodar simulacao`. O Studio cria:

```text
results/scenarios/<run_name>/
```

E grava:

```text
config_used.ini
summary.txt
population.csv
raster.csv
neuron_<id>.csv
```

O painel da direita mostra conexoes, spikes totais, spikes EXC/INH e timesteps
ativos.

## 8. Como gerar graficos

Depois de uma simulacao, clique em `Gerar graficos`.

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

Se o Studio nao encontrar Python valido, ele mostra uma mensagem curta pedindo
uma instalacao compativel com `pandas` e `matplotlib`.

Evite usar Python de jogos, caches, runtimes internos ou pastas como Steam,
WARNO e codex-runtimes. Quando o Studio estiver configurado, o terminal nao e
necessario para gerar graficos.

## 9. Como abrir resultados

Clique em `Abrir resultados` depois de rodar uma simulacao. A pasta da execucao
sera aberta no Explorer.

## 10. Como evitar sobrescrever uma execucao anterior

Os resultados sao gravados em modo de escrita. Rodar novamente o mesmo
`run_name` substitui os arquivos daquela pasta.

Para comparar testes:

1. Mude `Nome da execucao`.
2. Rode a simulacao.
3. Gere os graficos.
4. Compare as pastas em `results/scenarios/`.

## 11. O que ainda nao existe no Studio

Esta etapa nao implementa peixe, mundo, plasticidade, recompensa, punicao,
neuroevolucao ou topologia adaptativa.

O Studio apenas expoe os recursos ja existentes no motor de cenarios.

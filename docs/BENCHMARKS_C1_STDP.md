# Benchmarks locais do C1 STDP

Medição local controlada, não benchmark universal. Ambiente desta execução:
Windows 11 AMD64, GCC/MinGW 16.1 e Python 3.14.6. Foram usadas três repetições,
100 neurônios, 2.000 passos, topologia `random_balanced`, seed 23, densidade
0,05 e 521 conexões. Com STDP ativo, 423 conexões de origem EXC eram elegíveis.

```powershell
mingw32-make benchmark-c1
```

## Resultados observados

| Modo | Simulação média (s) | Steps/s | Updates/s | Elegíveis | Output |
|---|---:|---:|---:|---:|---:|
| STDP off | 0,025667 | 87.455 | 8.745.520 | 0 | 1.172.633 bytes |
| STDP on, sem histórico | 0,010333 | 86.111 | 8.611.111 | 423 | 273.625 bytes |
| STDP on, com histórico | 0,036333 | 57.195 | 5.719.515 | 423 | 1.502.932 bytes |

A geração de `plasticity_overview.png` levou 1,634821 s nessa execução.

## Como ler

`performance_simulation_time_seconds` mede o laço do runner, inclusive os CSVs
escritos durante os passos. STDP altera a dinâmica e, portanto, a quantidade de
linhas do raster; por isso a diferença entre OFF e ON não isola perfeitamente
somente aritmética do Core. O par ON sem/com histórico separa melhor o custo do
registro de `weight_history.csv` sob a mesma configuração inicial.

Os tempos são curtos e sujeitos à resolução do relógio, carga do sistema,
atividade produzida e cache. O resultado relevante é observacional: STDP
desligado não cria índice de entradas nem executa a regra; STDP ligado percorre
eventos pré/pós e o histórico acrescenta I/O proporcional à amostra e ao
intervalo. O custo do gráfico Python é separado da simulação.

Dados brutos recriáveis ficam em `results/benchmarks/stdp_c1.csv` e o ambiente
em `results/benchmarks/environment_c1.txt`. Eles não são golden de correção.

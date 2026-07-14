# Benchmarks C1.5 de homeostase

[Voltar ao índice](INDICE_DA_DOCUMENTACAO.md)

## Escopo

`scripts/run_benchmarks_c15.py` mede cinco modos equivalentes: homeostase OFF,
somente threshold, threshold com scaling, os três mecanismos e os três com
histórico. Todos usam 100 neurônios, 2000 passos, densidade 0.05, seed 23,
intervalo homeostático 10 e STDP ativo. Cada modo é repetido três vezes.

O alvo é reproduzível:

```powershell
mingw32-make benchmark-c15
```

Os resultados ficam em `results/benchmarks/homeostasis_c15.csv` e o ambiente em
`results/benchmarks/environment_c15.txt`. Runs e configs intermediárias são
removidas. Não existe limiar universal de desempenho.

## Medição local de 2026-07-13

Ambiente: Windows 11, processador AMD64 Family 26 Model 68, GCC MSYS2 16.1.0,
Python 3.14.6, base Git `1f5a62c` com alterações C1.5 locais.

| modo | wall médio (s) | steps/s | updates/s | saída (bytes) |
|---|---:|---:|---:|---:|
| homeostasis_off | 0.171779 | 11643 | 1164286 | 683290 |
| threshold_only | 0.168665 | 11858 | 1185784 | 847872 |
| threshold_scaling | 0.188812 | 10593 | 1059257 | 533353 |
| threshold_scaling_gain | 0.199640 | 10018 | 1001802 | 498872 |
| all_with_history | 0.202573 | 9873 | 987300 | 1526338 |

O ruído de processo é maior que diferenças pequenas entre os quatro primeiros
modos; não se deve interpretar uma execução ligeiramente mais rápida como ganho
de desempenho. O histórico aumentou claramente o volume de saída. No modo com
histórico, o gráfico levou 1.333 s e a geração dos relatórios HTML 0.086 s.

## Interpretação do custo

- `threshold_only` inclui traces e atualização periódica de limiares.
- `threshold_scaling` acrescenta a passagem pelo índice de entradas EXC.
- `threshold_scaling_gain` acrescenta a atualização escalar do ganho global.
- `all_with_history` acrescenta CSVs amostrados e o custo de I/O.
- Python é medido separadamente da simulação; o gráfico só é aplicável quando há histórico.

Wall time inclui inicialização, simulação e saída do runner. Os números são uma
medição local de engenharia, não uma garantia para outro hardware, compilador,
configuração de rede ou filesystem.

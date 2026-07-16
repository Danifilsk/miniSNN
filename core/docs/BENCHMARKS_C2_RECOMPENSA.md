# Benchmarks C2 de recompensa e R-STDP

[Voltar ao indice](INDICE_DA_DOCUMENTACAO.md)

## Escopo

`scripts/run_benchmarks_c2.py` compara sete modos controlados: plasticidade OFF,
STDP direto, R-STDP sem reward, reward esparso, reward em todo step, R-STDP com
homeostase e R-STDP com historico. O comando e:

```powershell
mingw32-make benchmark-c2
```

Os resultados locais ficam em `results/benchmarks/reward_c2.csv` e a descricao
do ambiente em `results/benchmarks/environment_c2.txt`. Configs e runs
intermediarias sao removidas.

## Metodologia

Cada modo usa 100 neuronios, 200 passos, densidade 0.05, seed 31 e tres
repeticoes. A duracao curta permite representar reward em todo step dentro do
limite fixo de 256 eventos do parser. Sao registrados wall time, steps/s,
updates neuronais/s, conexoes elegiveis, eventos, tamanho de saida e, quando
aplicavel, tempo do grafico e do HTML.

Os campos de custo de elegibilidade e aplicacao de reward sao apenas diferencas
aproximadas de wall time contra controles, nao perfis isolados. Ruido do sistema,
cache e I/O podem dominar diferencas pequenas. Nao existe threshold universal de
desempenho, e estes numeros nao sao garantia para outro hardware ou compilador.

## Resultados locais

Medição de 2026-07-13 em Windows 11, AMD64, GCC 16.1.0, Python 3.14.6 e base
`ffe49d0`:

| modo | wall médio (s) | steps/s | elegíveis | eventos | saída (bytes) |
|---|---:|---:|---:|---:|---:|
| plasticidade OFF | 0.128692 | 1554 | 0 | 0 | 16872 |
| STDP direto | 0.126675 | 1579 | 0 | 0 | 37361 |
| R-STDP sem reward | 0.134061 | 1492 | 409 | 0 | 72432 |
| R-STDP reward esparso | 0.140843 | 1420 | 409 | 3 | 86747 |
| R-STDP reward todo step | 0.156900 | 1275 | 409 | 200 | 1003278 |
| R-STDP + homeostase | 0.134835 | 1483 | 409 | 3 | 102133 |
| R-STDP + histórico | 0.141486 | 1414 | 409 | 3 | 359989 |

No modo com histórico, o gráfico levou 1.120 s e o HTML 0.080 s. As diferenças
são pequenas e ruidosas e não devem ser extrapoladas como custo universal.
Reward em todo step ampliou principalmente o tamanho dos outputs.

Execute novamente no ambiente de interesse e consulte o CSV. O arquivo de
ambiente registra plataforma, processador, Python, GCC, commit e configuração.

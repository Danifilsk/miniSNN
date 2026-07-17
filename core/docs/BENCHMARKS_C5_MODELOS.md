# Benchmarks C5 de modelos neuronais

Execute `mingw32-make benchmark-c5`. O resultado reproduzível local fica em
`results/benchmarks/c5_models.csv` e registra modelo, tamanho, conexões, passos,
`dt`, desempenho, estimativa de estado por neurônio, tamanho dos CSVs, plataforma,
compilador e commit.

Os casos cobrem LIF e AdEx pequeno/médio/grande e Hodgkin-Huxley pequeno/médio.
HH usa RK4 e não é comparado com LIF por um limite universal de desempenho.
As correntes têm unidades próprias de cada modelo; números iguais não implicam
equivalência física.

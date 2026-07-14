# Benchmarks C3 de neuroevolucao

[Voltar ao indice](INDICE_DA_DOCUMENTACAO.md)

## Escopo

`scripts/run_benchmarks_c3.py` mede localmente uma avaliacao isolada e pequenas
populacoes sem plasticidade, com STDP, R-STDP, homeostase, multiplas replicas e
`save_all_genomes` ligado/desligado. O modo com todos os genomas tambem mede a
geracao do PNG e do HTML.

```powershell
mingw32-make benchmark-c3 PYTHON="C:\caminho\para\python.exe"
```

Os resultados recriaveis ficam em `results/benchmarks/evolution_c3.csv`; o
ambiente fica em `results/benchmarks/environment_c3.txt`. Configuracoes e
execucoes intermediarias sao removidas e o ponteiro do ultimo experimento do
Studio e preservado.

## Metodologia

As populacoes usam 6 individuos, 3 geracoes e 180 passos por avaliacao. O CSV
registra individuos/s, avaliacoes/s, passos neurais/s, tempo por geracao, tempo
total, bytes dos CSVs, bytes totais, checkpoint/resume, PNG e HTML. O tempo de
checkpoint inclui a execucao ate a primeira fronteira de geracao: nao e um
perfil isolado de I/O.

O benchmark e serial e deterministico. Nao existe threshold universal. Cache,
antivirus, carga do sistema, hardware, compilador e sistema de arquivos podem
dominar diferencas pequenas; os valores sao observacoes locais, nao garantias.

## Resultados

Execute o alvo no ambiente de interesse e consulte os dois arquivos gerados.
O documento nao congela numeros de uma maquina como requisito de desempenho.

## Limites

O benchmark nao mede paralelismo, GPU, evolucao estrutural nem otimo global.
Geracao de relatorios depende do Python usado pelo comando e de `pandas` e
`matplotlib` para o panorama PNG.

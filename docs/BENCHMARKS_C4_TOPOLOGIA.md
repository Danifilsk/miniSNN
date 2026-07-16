# Benchmarks C4 — topologia adaptativa

O benchmark C4 é local, determinístico e não impõe limite universal de
desempenho.

```powershell
mingw32-make benchmark-c4
```

Ele compara doze modos:

1. C3 `fixed_numeric`;
2. C4 estrutural sem mutações;
3. add/remove;
4. crossover estrutural;
5. reachability ligada;
6. plasticidade estrutural desligada;
7. pruning;
8. growth;
9. STDP + estrutura;
10. R-STDP + estrutura;
11. homeostase + estrutura;
12. histórico estrutural ligado.

Os resultados ficam em:

```text
results/benchmarks/topology_c4.csv
results/benchmarks/environment_c4.txt
```

São registrados wall time, avaliações/s, steps/s, tempo por geração, tamanho do
checkpoint e CSVs, além de PNG/HTML nos modos que os geram. Fases internas que
o Core não cronometra separadamente aparecem como `NA`; o script não atribui o
wall time total artificialmente a crossover, mutação, validação, rebuild ou
manutenção.

O ambiente registra plataforma, processador, Python, compilador, commit e
configuração reduzida. Resultados variam com máquina, carga e toolchain.

A manutenção estrutural roda somente em
`maintenance_interval_steps`. O caminho amostrado de growth limita candidatos;
portanto, a implementação não faz varredura `O(N²)` a cada timestep. Redes
pequenas podem enumerar pares durante um intervalo de manutenção.

Essas medições caracterizam custo de engenharia. Não medem plausibilidade
biológica, qualidade da solução nem causalidade entre uma operação estrutural e
uma mudança comportamental.

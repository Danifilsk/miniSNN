# Benchmarks locais da Core v0.2

Medição controlada da máquina de desenvolvimento, não benchmark universal.
Topologia esparsa com uma saída por neurônio, três repetições e timeout de 120 s.
Ambiente: Windows 11, AMD64, GCC/MinGW 16.1 e baseline `48f119e`.

## Escala do núcleo público

| Neurônios | Passos | Conexões | Média (s) | Mín. | Máx. | Desvio | Updates/s |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 2000 | 100 | 0.014752 | 0.006711 | 0.030583 | 0.011195 | 13.56 milhões |
| 1000 | 500 | 1000 | 0.011551 | 0.010920 | 0.012394 | 0.000620 | 43.29 milhões |
| 10000 | 100 | 10000 | 0.019386 | 0.018837 | 0.019682 | 0.000389 | 51.58 milhões |

10.000 foi exercitado pela API pública; o parser interativo limita cenários a
1.000. Não foram executados 100.000 neurônios e nenhum caso grande foi all-to-all.

## Pipeline diagnóstico

Config: 100 neurônios, 500 passos, random balanced esparsa.

| Nível | Média (s) | Mín. | Máx. | Desvio | Output |
|---|---:|---:|---:|---:|---:|
| off | 0.120426 | 0.117721 | 0.123648 | 0.002447 | 157237 bytes |
| basic | 1.056573 | 1.043217 | 1.069772 | 0.010842 | 221939 bytes |
| full | 1.466795 | 1.456246 | 1.474514 | 0.007722 | 344400 bytes |

```powershell
mingw32-make benchmark-v02
```

Os dados brutos em `results/benchmarks/` são recriáveis e não são golden de
correção. Variações por carga do sistema são esperadas.

# Cobertura de testes

## Classificação

- **Numérico:** compara com referência independente.
- **Estrutural:** verifica pares, índices, contagens, formatos e invariantes.
- **Integração:** atravessa módulos, arquivos ou processos.
- **Regressão:** preserva resultado conhecido ou golden revisado.
- **Robustez:** exercita erros, ciclo de vida e dados adversos.
- **Benchmark:** mede execução local, sem promessa universal.
- **Smoke:** confirma fluxo, sem provar sua matemática.

| Área | Evidência | Força | Lacuna |
|---|---|---|---|
| LIF | `test_LIF` | numérica | outros modelos não existem |
| Rede e delay | API, core, memory stress | numérica/estrutural | injeção de falha de `malloc` |
| Topologias do Studio | `test_runner_topologies` | estrutural exata | GUI manual |
| Seed | `test_reproducibility` | regressão local | plataformas diferentes |
| Parser/runner | testes de cenário | robustez/integração | permissões reais globais |
| Métricas | testes Python | numérica | interpretação biológica |
| Regressão | golden de sete topologias | regressão | atualização exige revisão |
| Longa duração | `test-long` | robustez | proximidade do overflow de `int` |
| Memória | stress + analyzer | estática/robustez | ASan indisponível |

```powershell
mingw32-make test
mingw32-make test-diagnostics
mingw32-make test-regression
mingw32-make test-analyzer
mingw32-make test-sanitize
mingw32-make test-long
```

O golden só deve ser atualizado após revisão científica explícita:

```powershell
python scripts/capture_baseline.py --write
```


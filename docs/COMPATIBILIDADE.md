# Compatibilidade

## Configs

O parser começa com defaults e substitui apenas chaves presentes.

- Configs antigas sem `[output]` continuam válidas: `auto_unique_run = false` e `history_enabled = true`.
- Configs antigas sem `[diagnostics]` carregam `level = off`.
- Um cenário novo criado pelo Studio parte de `level = basic`.
- Seções são organizacionais; as chaves reconhecidas são validadas globalmente.
- Chaves desconhecidas ou duplicadas geram erro com número da linha.

Chaves acrescentadas ao formato atual:

```ini
[output]
auto_unique_run = false
history_enabled = true

[diagnostics]
level = off
time_bin_steps = 10
burst_z_threshold = 2.0
min_burst_steps = 1
isi_min_spikes = 4
correlation_sample_size = 128
neuron_sample_limit = 1000
sample_stride = 1
```

Valores de `level`: `off`, `basic` e `full`.

## Outputs

- **Base histórica:** `summary.txt`, `population.csv`, `raster.csv`, `neuron_<id>.csv` e `config_used.ini`.
- **Neurônio detalhado (A2):** gráfico individual opcional `neuron_<id>_detail.png`.
- **Comparação (A3):** pasta em `results/comparisons/` com resumo, relatório e gráficos.
- **Organização (A4):** `actual_run_name`, nomes únicos e arquivos `index.csv`.
- **Diagnóstico (A5):** manifesto, `metrics.csv`, relatório e gráficos `diagnostics_*`.
- **Relatórios HTML (C1.1):** `metrics_report.html` e, quando há pesos registrados, `weights_report.html`.

Runs antigas sem `metrics.csv` continuam comparáveis por fallback quando os CSVs
necessários existem. Dados ausentes resultam em aviso ou `NA`, não em valores inventados.

Mudança de interface: `ABRIR METRICAS` e `ABRIR PESOS` deixam de abrir
diretamente `metrics.csv` e `weights_final.csv`; agora abrem os relatórios HTML
locais. Os CSVs continuam sendo produzidos, não mudaram de formato e permanecem
acessíveis pela pasta e pelos links internos. Uma run antiga com `metrics.csv`
pode receber `metrics_report.html` posteriormente. Runs sem
`run_manifest.txt` também são aceitas. Com STDP desligado ou sem
`weights_final.csv`, a ausência de `weights_report.html` é esperada e tratada
com mensagem clara.

Na v0.2, `summary.txt` recebeu assinatura e contagens por classe de conexão sem
remover chaves antigas. `NaN`/infinito em colunas obrigatórias agora gera erro ou
descarte explícito, em vez de contaminar métricas. Histórico sem o cabeçalho
esperado é recusado e não recebe novas linhas silenciosamente.

## API pública

A API atual está em `include/minisnn.h` e usa o tipo opaco `MiniSNN`. As funções
`minisnn_connect_ex()` e `minisnn_connect_delayed_ex()` acrescentam controle
explícito de auto-conexão. As funções sem `_ex` continuam rejeitando self-loop.

O C1 acrescenta inspeção determinística de conexões, pesos mutáveis e API de
plasticidade sem remover funções existentes. Cada rede mantém configuração,
traces e estatísticas próprios.

Não há ainda política de versionamento semântico formal. Mudanças futuras devem
ser documentadas e funções públicas devem ser deprecadas antes de remoção.

## Plataforma

- O Studio é Win32.
- O build principal é Windows/MSYS2 com GCC/MinGW e `mingw32-make`.
- Scripts de análise usam Python; alguns exigem pandas e matplotlib.
- O Core em C pode ser portável, mas outros sistemas não fazem parte da validação atual.

## Resultados científicos

Mudanças futuras na dinâmica, fórmulas, topologias ou semântica dos CSVs devem
receber nota de compatibilidade e, quando necessário, identificação de nova
versão. Resultados antigos devem manter config, seed, commit e manifesto sempre
que disponíveis.

Os totais `random_demo = 6757` e `small_world_demo = 15045` foram preservados na
auditoria da Core v0.2.

Configs sem `[plasticity]` e configs com `enabled = false` são equivalentes no
estado dinâmico normalizado. Elas não geram arquivos pesados de pesos. Ativar
STDP muda pesos e pode mudar spikes, portanto resultados C1 devem preservar
config, seed, manifesto e arquivos de plasticidade.

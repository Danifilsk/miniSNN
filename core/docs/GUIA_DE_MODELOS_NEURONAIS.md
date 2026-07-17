# Guia de modelos neuronais

A miniSNN suporta redes homogêneas com três modelos. O modelo escolhido em
`[neuron]` permanece fixo durante a execução e a neuroevolução.
Esse suporte foi concluído no C5.

| Modelo | Nome canônico | Integrador | Estado específico | Threshold homeostático |
|---|---|---|---|---|
| LIF | `lif` | Euler histórico | nenhum | sim |
| Adaptive Exponential IF | `adex` | Euler explícito | `w` | não |
| Hodgkin-Huxley clássico | `hodgkin_huxley` | RK4 | `m`, `h`, `n` | não |

Aliases aceitos sem distinção de caixa: `lif`, `adex`, `hh`,
`hodgkin_huxley` e `hodgkin-huxley`. A saída sempre usa o nome canônico.
Para todos os modelos, `MiniSNNConfig.dt` é a fonte autoritativa do timestep.
O `dt` dentro das seções `[adex]` e `[hodgkin_huxley]` é preservado como parte
da configuração editável, mas a rede criada usa o valor superior efetivo.

## LIF

O LIF preserva equação, ordem temporal e defaults históricos. Corrente e pesos
usam as unidades históricas do projeto.

## AdEx

O AdEx usa adaptação `w`, termo exponencial limitado de forma segura e reset em
`V_peak`. A corrente é interpretada em pA. Parâmetros: `capacitance`, `g_leak`,
`e_leak`, `delta_t`, `v_threshold`, `tau_w`, `a`, `b`, `v_reset` e `v_peak`.
O runner gera `adex_state.csv`.

## Hodgkin-Huxley

O HH implementa o modelo clássico de axônio de lula em voltagem absoluta, com
taxas estáveis nos pontos singulares e RK4 para `V,m,h,n`. Não há reset
artificial; spike é o cruzamento ascendente do limiar. Corrente e pesos são
densidades em µA/cm². Use `dt` pequeno; o default é `0.01` ms. Parâmetros:
`capacitance`, `g_na`, `g_k`, `g_leak`, `e_na`, `e_k`, `e_leak`, `v_init` e
`spike_threshold`. O runner gera `hh_state.csv`.

## Capabilities e segurança

Os três modelos oferecem potencial e evento de spike. Apenas LIF oferece
threshold homeostático adaptativo. Scaling sináptico e ganho inibitório podem
ser usados com os três. O próximo estado é calculado temporariamente e só é
aplicado quando finito; erro interrompe rede, cenário e avaliação.

Cada configuração possui assinatura FNV-1a versionada baseada no modelo,
integrador e parâmetros ativos. Blueprints C5 incluem essa assinatura.
Checkpoints C4 usam a assinatura legada e só são aceitos em cenários LIF
compatíveis.

Correntes e pesos numericamente iguais não têm necessariamente significado
físico equivalente entre modelos. Nenhum modelo é universalmente melhor.

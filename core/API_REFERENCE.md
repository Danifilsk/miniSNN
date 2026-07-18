# Referência da API pública miniSNN

[Voltar ao índice da documentação](docs/INDICE_DA_DOCUMENTACAO.md)

**Status:** implementado. A fonte de verdade das assinaturas é
`include/minisnn.h`; este documento foi conferido com o header atual no C1.5.

Esta referência descreve a API pública atual de `minisnn.h`.

O C3 não altera as assinaturas deste header. Sua superfície pública experimental
é o executável `build/evolution_runner.exe`, os arquivos INI com `[evolution]`,
`[genome]` e `[fitness]`, e os artefatos documentados no
[Guia de neuroevolução](docs/GUIA_DE_NEUROEVOLUCAO.md). O motor em
`src/evolution.h` permanece interno para preservar o encapsulamento da API C.

`MiniSNN` e um tipo opaco. O usuario cria, usa e destroi redes por funcoes
publicas, sem acessar campos internos.

Funcoes booleanas retornam `1` em sucesso e `0` em falha. `minisnn_step`
retorna o numero de spikes no timestep, ou `-1` em erro.

## MiniSNNConfig

**Objetivo:** configurar uma instancia de rede no momento da criacao.

**Campos:**

- `neuron_count`: numero de neuronios.
- `neuron_model`: modelo neuronal da instancia: LIF, AdEx ou
  Hodgkin-Huxley. A configuracao padrao seleciona LIF.
- `dt`: passo de tempo autoritativo do modelo selecionado.
- `tau`: constante de tempo.
- `v_rest`: potencial de repouso.
- `v_reset`: potencial de reset.
- `v_threshold`: limiar de spike.
- `resistance`: resistencia usada no LIF.
- `synaptic_decay`: decaimento da corrente sinaptica.
- `max_synaptic_delay`: maior delay sinaptico permitido.

**Erros importantes:** configuracoes invalidas fazem
`minisnn_create_with_config` retornar `NULL`.

## Modelo neuronal

`MiniSNNNeuronModel` deixa explicito o modelo de uma rede. Os valores
implementados sao `MINISNN_NEURON_MODEL_LIF`, `MINISNN_NEURON_MODEL_ADEX` e
`MINISNN_NEURON_MODEL_HODGKIN_HUXLEY`. Cada rede permanece homogenea durante
sua vida; `minisnn_neuron_model()` consulta o valor da instancia.

## minisnn_default_config

```c
MiniSNNConfig minisnn_default_config(void);
```

**Objetivo:** obter a configuracao padrao da biblioteca.

**Parametros:** nenhum.

**Retorno:** uma `MiniSNNConfig` preenchida com valores padrao.

**Erros importantes:** nao retorna erro.

## minisnn_create

```c
MiniSNN *minisnn_create(int neuron_count);
```

**Objetivo:** criar uma rede com configuracao padrao e tamanho escolhido.

**Parametros:** `neuron_count`, numero de neuronios.

**Retorno:** ponteiro para `MiniSNN`, ou `NULL` em falha.

**Erros importantes:** retorna `NULL` se `neuron_count <= 0` ou se houver
falha de alocacao/inicializacao.

## minisnn_create_with_config

```c
MiniSNN *minisnn_create_with_config(const MiniSNNConfig *config);
```

**Objetivo:** criar uma rede com configuracao por instancia.

**Parametros:** `config`, ponteiro para a configuracao desejada.

**Retorno:** ponteiro para `MiniSNN`, ou `NULL` em falha.

**Erros importantes:** retorna `NULL` se `config == NULL`, se algum parametro
for invalido ou se houver falha de alocacao/inicializacao. A configuracao e
copiada para a rede no momento da criacao.

## minisnn_destroy

```c
void minisnn_destroy(MiniSNN **snn_ptr);
```

**Objetivo:** liberar uma rede.

**Parametros:** `snn_ptr`, endereco do ponteiro para a rede.

**Retorno:** nenhum.

**Erros importantes:** e seguro chamar com `NULL`, com ponteiro ja nulo ou mais
de uma vez. `minisnn_destroy(&brain)` libera a rede e define `brain` como
`NULL`.

## minisnn_neuron_count

```c
int minisnn_neuron_count(const MiniSNN *snn);
```

**Objetivo:** consultar o numero de neuronios.

**Parametros:** `snn`, rede.

**Retorno:** numero de neuronios, ou `0` se `snn == NULL`.

**Erros importantes:** rede nula retorna `0`.

## minisnn_current_step

```c
int minisnn_current_step(const MiniSNN *snn);
```

**Objetivo:** consultar o timestep atual.

**Parametros:** `snn`, rede.

**Retorno:** timestep atual, ou `-1` se `snn == NULL`.

**Erros importantes:** rede nula retorna `-1`.

## minisnn_neuron_model

```c
MiniSNNNeuronModel minisnn_neuron_model(const MiniSNN *snn);
```

**Objetivo:** consultar o modelo neuronal configurado na instancia.

**Retorno:** o modelo da rede; para ponteiro nulo, retorna o valor seguro
padrao `MINISNN_NEURON_MODEL_LIF`.

## MiniSNNConnectionInfo

Visão pública de uma conexão: `source`, `target`, tipos, `weight`, `delay` e
`plasticity_eligible`. IDs planos são determinísticos: origem crescente e ordem
de inserção na lista de saída.

## MiniSNNPlasticityConfig

Configura o STDP: `enabled`, `rule`, `a_plus`, `a_minus`, `tau_plus`,
`tau_minus`, `trace_increment`, `weight_min` e `weight_max`. O C1 aceita
`MINISNN_PLASTICITY_STDP_PAIR_TRACE`; a regra é aditiva e somente origens EXC
são elegíveis.

## MiniSNNPlasticityStats

Contém eventos LTP/LTD, clamps mínimo/máximo, conexões elegíveis/modificadas e
mudanças assinada, absoluta e máxima acumuladas.

## minisnn_connection_count

```c
size_t minisnn_connection_count(const MiniSNN *snn);
```

Retorna a quantidade de conexões, ou zero para rede nula/inválida.

## minisnn_get_connection

```c
int minisnn_get_connection(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNConnectionInfo *out_connection);
```

Preenche uma visão estável da conexão indicada. Retorna zero para ponteiros ou
ID inválidos.

## minisnn_get_connection_weight

```c
int minisnn_get_connection_weight(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_weight);
```

Consulta o peso atual sem expor o armazenamento interno.

## minisnn_set_connection_weight

```c
int minisnn_set_connection_weight(
    MiniSNN *snn,
    size_t connection_id,
    double weight);
```

Substitui o peso atual quando o ID existe e o valor é finito. A função não
aplica os limites STDP a uma edição manual; o chamador continua responsável
pelo sinal desejado. Uma conexão EXC com peso negativo deixa de ser elegível.

## minisnn_default_plasticity_config

```c
MiniSNNPlasticityConfig minisnn_default_plasticity_config(void);
```

Retorna configuração válida com plasticidade desligada.

## minisnn_set_plasticity_config

```c
int minisnn_set_plasticity_config(
    MiniSNN *snn,
    const MiniSNNPlasticityConfig *config);
```

Valida e copia a configuração. Preserva pesos, zera traces e estatísticas e
reconstrói o índice de entradas quando necessário. Com STDP ativo, a estrutura
de conexões deve estar pronta antes do primeiro step.

## minisnn_get_plasticity_config

```c
int minisnn_get_plasticity_config(
    const MiniSNN *snn,
    MiniSNNPlasticityConfig *out_config);
```

Consulta a configuração efetiva da instância.

## minisnn_get_plasticity_stats

```c
int minisnn_get_plasticity_stats(
    const MiniSNN *snn,
    MiniSNNPlasticityStats *out_stats);
```

Consulta estatísticas acumuladas desde a última configuração da plasticidade.

## minisnn_get_plasticity_traces

```c
int minisnn_get_plasticity_traces(
    const MiniSNN *snn,
    int neuron_id,
    double *out_pre_trace,
    double *out_post_trace);
```

Consulta os traces pré e pós de um neurônio. Ambos os destinos são obrigatórios.
Traces começam em zero e usam o `dt` da instância.

## MiniSNNHomeostasisConfig

Configura a homeostase opcional por instância. Contém ativação global e dos três
mecanismos, taxa-alvo e tau, intervalo, parâmetros e clamps de threshold,
scaling EXC e ganho INH. A configuração padrão é válida e vem desligada.

## MiniSNNHomeostasisStats

Contém contadores separados de atualizações, mudanças e clamps de threshold,
scaling e ganho, eventos de soma zero, mudanças acumuladas, agregados da taxa,
fatores de scaling observados e estado populacional final.

## minisnn_default_homeostasis_config

```c
MiniSNNHomeostasisConfig minisnn_default_homeostasis_config(void);
```

Retorna os defaults documentados no guia de homeostase, com `enabled = 0`.

## minisnn_set_homeostasis_config

```c
int minisnn_set_homeostasis_config(
    MiniSNN *snn,
    const MiniSNNHomeostasisConfig *config);
```

Valida e copia a configuração. Preserva pesos, potenciais, step e STDP; zera
traces e estatísticas, restaura thresholds e ganho e recaptura as somas EXC.

## minisnn_get_homeostasis_config

```c
int minisnn_get_homeostasis_config(
    const MiniSNN *snn,
    MiniSNNHomeostasisConfig *out_config);
```

Consulta a configuração efetiva sem expor o estado interno.

## minisnn_reset_homeostasis

```c
int minisnn_reset_homeostasis(MiniSNN *snn);
```

Reaplica a semântica de reset à configuração atual. Preserva pesos e recaptura
os alvos de scaling a partir deles.

## minisnn_get_homeostasis_stats

```c
int minisnn_get_homeostasis_stats(
    const MiniSNN *snn,
    MiniSNNHomeostasisStats *out_stats);
```

Consulta estatísticas acumuladas desde a última configuração ou reset.

## minisnn_get_neuron_rate_trace

```c
int minisnn_get_neuron_rate_trace(
    const MiniSNN *snn,
    int neuron_id,
    double *out_rate_trace);
```

Retorna a taxa exponencial do neurônio. Com homeostase desligada, retorna zero.

## minisnn_get_neuron_effective_threshold

```c
int minisnn_get_neuron_effective_threshold(
    const MiniSNN *snn,
    int neuron_id,
    double *out_threshold);
```

Retorna o threshold individual usado em passos futuros. Desligada, coincide com
o threshold-base.

## minisnn_get_base_threshold

```c
int minisnn_get_base_threshold(
    const MiniSNN *snn,
    double *out_threshold);
```

Retorna o `v_threshold` imutável da configuração neural da instância.

## minisnn_get_inhibitory_gain

```c
int minisnn_get_inhibitory_gain(
    const MiniSNN *snn,
    double *out_gain);
```

Retorna o multiplicador global de transmissão INH. Desligada, retorna `1.0`.

## minisnn_get_initial_incoming_exc_sum

```c
int minisnn_get_initial_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);
```

Retorna o alvo capturado para o scaling EXC do neurônio.

## minisnn_get_current_incoming_exc_sum

```c
int minisnn_get_current_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);
```

Calcula a soma atual das entradas positivas de origem EXC usando o índice
interno de conexões de entrada.

## MiniSNNNeuronType

```c
typedef enum
{
    MINISNN_NEURON_EXCITATORY = 0,
    MINISNN_NEURON_INHIBITORY = 1
} MiniSNNNeuronType;
```

**Objetivo:** representar o tipo publico de neuronio usado pela API.

**Valores:**

- `MINISNN_NEURON_EXCITATORY`: neuronio excitatorio.
- `MINISNN_NEURON_INHIBITORY`: neuronio inibitorio.

## minisnn_set_neuron_type

```c
int minisnn_set_neuron_type(
    MiniSNN *snn,
    int neuron_id,
    MiniSNNNeuronType type);
```

**Objetivo:** definir se um neuronio e excitatorio ou inibitorio.

**Parametros:** `snn`, rede; `neuron_id`, indice do neuronio; `type`, tipo.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido ou tipo invalido.

## minisnn_connect

```c
int minisnn_connect(MiniSNN *snn, int source, int target, double weight);
```

**Objetivo:** criar uma conexao com delay padrao de 1 timestep.

**Parametros:** `snn`, rede; `source`, origem; `target`, destino; `weight`,
peso sinaptico.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indices invalidos, auto-conexao,
peso nao finito ou conexao duplicada.

## minisnn_connect_ex

```c
int minisnn_connect_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int allow_self_connection);
```

**Objetivo:** criar uma conexao com delay padrao de 1 timestep, permitindo
auto-conexao apenas quando `allow_self_connection` for diferente de zero.

**Retorno:** `1` em sucesso, `0` em falha.

**Observacao:** `minisnn_connect()` continua rejeitando auto-conexoes. Use esta
funcao somente quando o self-loop for uma escolha explicita do experimento.

## minisnn_connect_delayed

```c
int minisnn_connect_delayed(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay);
```

**Objetivo:** criar uma conexao com delay especifico.

**Parametros:** `snn`, rede; `source`, origem; `target`, destino; `weight`,
peso; `delay`, atraso em timesteps.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indices invalidos, auto-conexao,
peso nao finito, delay fora do intervalo permitido ou conexao duplicada.

## minisnn_connect_delayed_ex

```c
int minisnn_connect_delayed_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection);
```

**Objetivo:** criar uma conexao com delay especifico e controle explicito de
auto-conexao.

**Comportamento:** quando `allow_self_connection == 0`, e equivalente a
`minisnn_connect_delayed()` e rejeita `source == target`. Quando
`allow_self_connection != 0`, `source == target` pode ser aceito, mantendo as
validacoes de indices, peso finito, delay valido e duplicatas.

## minisnn_set_input

```c
int minisnn_set_input(MiniSNN *snn, int neuron_id, double current);
```

**Objetivo:** substituir a corrente externa de um neuronio.

**Parametros:** `snn`, rede; `neuron_id`, indice; `current`, corrente.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido ou corrente nao
finita.

## minisnn_add_input

```c
int minisnn_add_input(MiniSNN *snn, int neuron_id, double current);
```

**Objetivo:** somar corrente externa a um neuronio.

**Parametros:** `snn`, rede; `neuron_id`, indice; `current`, corrente a somar.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido, corrente nao
finita ou resultado nao finito.

## minisnn_clear_inputs

```c
void minisnn_clear_inputs(MiniSNN *snn);
```

**Objetivo:** zerar todas as correntes externas.

**Parametros:** `snn`, rede.

**Retorno:** nenhum.

**Erros importantes:** chamada com rede nula e segura e nao faz nada.

## minisnn_step

```c
int minisnn_step(MiniSNN *snn);
```

**Objetivo:** executar um timestep da simulacao.

**Parametros:** `snn`, rede.

**Retorno:** numero de spikes no timestep, ou `-1` em erro.

**Erros importantes:** retorna `-1` se a rede for nula ou invalida.

## minisnn_get_spike

```c
int minisnn_get_spike(const MiniSNN *snn, int neuron_id, int *out_spike);
```

**Objetivo:** consultar se um neuronio disparou no timestep mais recente.

**Parametros:** `snn`, rede; `neuron_id`, indice; `out_spike`, destino do valor.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido ou `out_spike ==
NULL`.

## minisnn_get_voltage

```c
int minisnn_get_voltage(const MiniSNN *snn, int neuron_id, double *out_voltage);
```

**Objetivo:** consultar o potencial de membrana mais recente.

**Parametros:** `snn`, rede; `neuron_id`, indice; `out_voltage`, destino do
valor.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido ou `out_voltage ==
NULL`.

## minisnn_get_synaptic_current

```c
int minisnn_get_synaptic_current(
    const MiniSNN *snn,
    int neuron_id,
    double *out_current);
```

**Objetivo:** consultar a corrente sinaptica usada no timestep mais recente.

**Parametros:** `snn`, rede; `neuron_id`, indice; `out_current`, destino do
valor.

**Retorno:** `1` em sucesso, `0` em falha.

**Erros importantes:** falha com rede nula, indice invalido ou `out_current ==
NULL`. A corrente retornada e a corrente efetivamente usada pelo LIF no
timestep mais recente.

## MiniSNNLearningMode

```c
typedef enum {
    MINISNN_LEARNING_MODE_DIRECT_STDP = 0,
    MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP = 1
} MiniSNNLearningMode;
```

Seleciona entre a atualização imediata do C1 e o acúmulo de elegibilidade do
R-STDP. O campo `learning_mode` de `MiniSNNPlasticityConfig` usa essa enumeração.

## MiniSNNRewardConfig

Configuração por rede do sinal modulador: `enabled`, modo `rstdp`,
`learning_rate`, constante e limites de elegibilidade, limites de reward e
política `clip_reward`. Todos os valores reais devem ser finitos. O modo reward
requer plasticidade ativa em
`MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP`.

## MiniSNNRewardStats

Estatísticas globais de eventos positivos, negativos e zero; componentes;
clamps; conexões elegíveis/modificadas; reward bruto/aplicado; mudanças de peso;
e distribuição final da elegibilidade. Mudanças contabilizadas aqui são apenas
as produzidas por reward, separadas do STDP direto e do scaling.

## MiniSNNRewardConnectionStats

Estado observável de uma conexão: elegibilidade atual, maior elegibilidade
absoluta, número de atualizações por reward e mudanças de peso acumuladas. Uma
conexão de origem INH retorna `eligible = 0`.

## minisnn_default_reward_config

```c
MiniSNNRewardConfig minisnn_default_reward_config(void);
```

Retorna reward desativado, modo R-STDP, `learning_rate = 1`, tau e bounds
seguros. A estrutura retornada pode ser editada antes de ser aplicada.

## minisnn_set_reward_config

```c
int minisnn_set_reward_config(
    MiniSNN *snn,
    const MiniSNNRewardConfig *config);
```

Valida e configura o estado opcional por conexão. Ao reconfigurar, preserva
pesos e estado neural e reinicia elegibilidades e estatísticas do módulo.
Retorna `1` em sucesso e `0` para rede/configuração inválida ou combinação
incompatível com a plasticidade.

## minisnn_get_reward_config

```c
int minisnn_get_reward_config(
    const MiniSNN *snn,
    MiniSNNRewardConfig *out_config);
```

Copia a configuração atual para `out_config`. Retorna `0` para ponteiros
inválidos.

## minisnn_queue_reward

```c
int minisnn_queue_reward(MiniSNN *snn, double value);
```

Acumula um componente finito para o próximo step. Com clipping, o valor bruto é
limitado uma única vez na aplicação; sem clipping, uma soma fora dos bounds é
recusada sem alterar o pendente. Reward desativado também é recusado.

## minisnn_get_pending_reward

```c
int minisnn_get_pending_reward(const MiniSNN *snn, double *out_value);
```

Retorna a soma bruta pendente. O pulso é consumido após o próximo
`minisnn_step`.

## minisnn_clear_pending_reward

```c
int minisnn_clear_pending_reward(MiniSNN *snn);
```

Descarta reward e componentes ainda não aplicados, sem alterar pesos,
elegibilidades ou estatísticas passadas.

## minisnn_get_last_applied_reward

```c
int minisnn_get_last_applied_reward(
    const MiniSNN *snn,
    double *out_value);
```

Retorna o sinal limitado efetivamente aplicado no último step com consumo.

## minisnn_reset_reward_learning

```c
int minisnn_reset_reward_learning(MiniSNN *snn);
```

Zera eligibilidades, reward pendente/aplicado e estatísticas de reward.
Preserva pesos, potenciais, step, configuração STDP e estado homeostático.

## minisnn_get_reward_stats

```c
int minisnn_get_reward_stats(
    const MiniSNN *snn,
    MiniSNNRewardStats *out_stats);
```

Materializa o decaimento preguiçoso no step atual e devolve estatísticas
globais finitas. A materialização é matematicamente equivalente ao decaimento
passo a passo.

## minisnn_get_connection_eligibility

```c
int minisnn_get_connection_eligibility(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_eligibility);
```

Consulta a elegibilidade pelo ID determinístico usado em
`minisnn_get_connection`. Falha para ID inválido ou reward inativo.

## minisnn_get_reward_connection_stats

```c
int minisnn_get_reward_connection_stats(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNRewardConnectionStats *out_stats);
```

Consulta elegibilidade e acumulados de uma conexão sem alterar seu resultado
matemático futuro.

## minisnn_reward_eligible_connection_count

```c
int minisnn_reward_eligible_connection_count(
    const MiniSNN *snn,
    size_t *out_count);
```

Retorna a quantidade de conexões de origem EXC acompanhadas pelo módulo. Em
modo direto ou reward desligado, retorna zero em `out_count` quando a consulta
é válida.

# Topologia adaptativa C4

Os tipos `MiniSNNConnectionGene`, `MiniSNNTopologyOperation`,
`MiniSNNTopologyPatchResult`, `MiniSNNStructuralPlasticityConfig`,
`MiniSNNStructuralStats`, `MiniSNNStructuralConnectionState` e
`MiniSNNStructuralEvent` estão declarados em `include/minisnn_types.h`.

## minisnn_connection_key

```c
int minisnn_connection_key(
    size_t neuron_count,
    size_t source,
    size_t target,
    uint64_t *out_key);
```

Calcula `source * neuron_count + target` com validação de endpoints e overflow.

## minisnn_validate_topology_patch

```c
int minisnn_validate_topology_patch(
    const MiniSNN *snn,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);
```

Valida atomicamente `ADD`, `REMOVE`, `REWIRE` e `SET_DELAY` sem alterar a rede.

## minisnn_apply_topology_patch

```c
int minisnn_apply_topology_patch(
    MiniSNN *snn,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);
```

Aplica o lote somente se a topologia final for válida. Sobreviventes preservam
estado por key; conexões novas começam sem elegibilidade retroativa.

## Configuração estrutural

```c
MiniSNNStructuralPlasticityConfig
minisnn_default_structural_plasticity_config(void);

int minisnn_set_structural_plasticity_config(
    MiniSNN *snn,
    const MiniSNNStructuralPlasticityConfig *config);

int minisnn_get_structural_plasticity_config(
    const MiniSNN *snn,
    MiniSNNStructuralPlasticityConfig *out_config);
```

O estado estrutural de lifetime só é alocado quando a configuração habilitada é
aplicada.

## Consultas estruturais

```c
int minisnn_get_structural_stats(
    const MiniSNN *snn,
    MiniSNNStructuralStats *out_stats);

int minisnn_get_topology_signature(
    const MiniSNN *snn,
    uint64_t *out_signature);

int minisnn_get_structural_connection_state(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNStructuralConnectionState *out_state);

size_t minisnn_structural_event_count(const MiniSNN *snn);

int minisnn_get_structural_event(
    const MiniSNN *snn,
    size_t event_index,
    MiniSNNStructuralEvent *out_event);
```

## minisnn_reset_structural_plasticity

```c
int minisnn_reset_structural_plasticity(
    MiniSNN *snn,
    MiniSNNStructuralResetMode mode);
```

`MINISNN_STRUCTURAL_RESET_STATE` preserva a topologia atual.
`MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY` restaura arestas, pesos e delays
capturados ao ativar o módulo.

## minisnn_default_structural_plasticity_config

Retorna defaults seguros para o módulo estrutural, inicialmente desabilitado.

## minisnn_set_structural_plasticity_config

Valida e ativa/desativa poda e crescimento; a alocação estrutural é opcional.

## minisnn_get_structural_plasticity_config

Copia a configuração estrutural ativa para o chamador.

## minisnn_get_structural_stats

Consulta manutenções, tentativas, sucessos, rebuilds e contagens de conexões.

## minisnn_get_topology_signature

Obtém a assinatura FNV-1a da topologia corrente, sem incluir pesos.

## minisnn_get_structural_connection_state

Consulta idade, uso e origem de crescimento de uma conexão enumerada.

## minisnn_structural_event_count

Retorna quantos eventos estruturais estão disponíveis para consulta.

## minisnn_get_structural_event

Copia um evento estrutural pelo índice sem expor armazenamento interno.

## Modelos neuronais C5

A API pública suporta redes homogêneas LIF, AdEx e Hodgkin-Huxley. O modelo é
definido por `MiniSNNConfig.neuron_model` e permanece fixo durante a vida da
rede.

### minisnn_adex_config_default

Retorna os parametros padrao reproduziveis do modelo AdEx.

### minisnn_hodgkin_huxley_config_default

Retorna os parametros padrao do modelo Hodgkin-Huxley classico.

### minisnn_neuron_model_name

Retorna o nome canonico `lif`, `adex` ou `hodgkin_huxley`.

### minisnn_neuron_model_capabilities

Informa suporte a potencial, spike, threshold homeostatico, adaptacao e gates.

### minisnn_neuron_model_config_signature

Retorna a assinatura versionada da configuracao ativa de uma rede.

### minisnn_config_neuron_model_signature

Calcula a mesma assinatura diretamente de `MiniSNNConfig`. O campo superior
`MiniSNNConfig.dt` é a fonte autoritativa do timestep para LIF, AdEx e
Hodgkin-Huxley; os campos `dt` aninhados existem para defaults e serialização,
mas não alteram a configuração efetiva criada pela API pública.

### minisnn_neuron_integration_method

Retorna o integrador da rede (`euler` ou `rk4`).

### minisnn_neuron_model_integration_method

Retorna o integrador associado a um valor de `MiniSNNNeuronModel`.

### minisnn_get_adex_state

Le `V`, adaptacao `w` e spike de um neuronio AdEx.

### minisnn_get_hodgkin_huxley_state

Le `V`, gates `m`, `h`, `n` e spike de um neuronio Hodgkin-Huxley.

## Interface cerebro-agente C7.1

`include/minisnn_agent_io.h` define uma API publica independente da rede
neural. Ela descreve canais numericos, schemas opacos, frames com ownership
explicito e uma maquina de estados por tick. Valores fora dos limites sao
rejeitados, nunca clampados silenciosamente.

### Schemas e assinaturas

`minisnn_sensor_schema_create` e `minisnn_action_schema_create` copiam ids,
nomes e limites; a ordem fornecida pelo chamador e a ordem canonica do frame.
Os schemas exigem ao menos um canal, ids e nomes unicos e valores finitos.
Nomes seguem ASCII imprimivel (`0x20..0x7E`); controle, `DEL` e bytes estendidos
sao rejeitados.

`minisnn_sensor_schema_signature`, `minisnn_action_schema_signature` e
`minisnn_agent_io_contract_signature` usam FNV-1a 64-bit padrao com serializacao
explicita da versao, quantidade, id, nome, minimo, maximo e default.

### Frames e ciclo por tick

Inicialize `MiniSNNSensorFrame` e `MiniSNNActionFrame` com `{0}` e use as
funcoes `*_frame_init`, `*_frame_set_values`, `*_frame_reset` e
`*_frame_destroy`. O contexto criado por `minisnn_agent_io_create` copia os
schemas e tambem copia cada frame submetido.

O ciclo valido e `submit_sensor_frame(tick)`, `consume_sensor_frame`,
`submit_action_frame(mesmo tick)`, `finish_tick` e `consume_action_frame`.
Os consumes copiam para buffers inicializados pelo chamador e cada frame pode
ser consumido uma unica vez. A action exige que o sensor pendente ja tenha sido
consumido, e um novo tick e bloqueado ate que a action finalizada anterior seja
consumida. Um sensor duplicado, consumo duplicado, acao antecipada, tick
regressivo ou acao com tick diferente retorna erro consultavel por
`minisnn_agent_io_last_error` e `minisnn_agent_io_error_string` sem alterar o
estado ja aceito.

### Texto estavel

`minisnn_sensor_schema_write_file`/`read_file` e as variantes de action
produzem schemas versionados e legiveis. O formato usa bits hexadecimais para
valores double e nao depende do locale; arquivo incompativel e rejeitado.
Caracteres de nome fora de `A-Z`, `a-z`, `0-9`, `_`, `-` e `.` sao
percent-encoded no texto estavel.

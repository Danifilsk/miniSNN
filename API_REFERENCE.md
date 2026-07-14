# Referência da API pública miniSNN

[Voltar ao índice da documentação](docs/INDICE_DA_DOCUMENTACAO.md)

**Status:** implementado. A fonte de verdade das assinaturas é
`include/minisnn.h`; este documento foi conferido com o header atual no C1.5.

Esta referência descreve a API pública atual de `minisnn.h`.

`MiniSNN` e um tipo opaco. O usuario cria, usa e destroi redes por funcoes
publicas, sem acessar campos internos.

Funcoes booleanas retornam `1` em sucesso e `0` em falha. `minisnn_step`
retorna o numero de spikes no timestep, ou `-1` em erro.

## MiniSNNConfig

**Objetivo:** configurar uma instancia de rede no momento da criacao.

**Campos:**

- `neuron_count`: numero de neuronios.
- `dt`: passo de tempo do LIF.
- `tau`: constante de tempo.
- `v_rest`: potencial de repouso.
- `v_reset`: potencial de reset.
- `v_threshold`: limiar de spike.
- `resistance`: resistencia usada no LIF.
- `synaptic_decay`: decaimento da corrente sinaptica.
- `max_synaptic_delay`: maior delay sinaptico permitido.

**Erros importantes:** configuracoes invalidas fazem
`minisnn_create_with_config` retornar `NULL`.

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

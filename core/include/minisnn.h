#ifndef MINISNN_H
#define MINISNN_H

#include "minisnn_types.h"
#include "minisnn_agent_io.h"

typedef struct MiniSNN MiniSNN;

typedef struct
{
    int neuron_count;
    double dt;
    double tau;
    double v_rest;
    double v_reset;
    double v_threshold;
    double resistance;

    double synaptic_decay;
    int max_synaptic_delay;

    MiniSNNNeuronModel neuron_model;
    MiniSNNAdExConfig adex;
    MiniSNNHodgkinHuxleyConfig hodgkin_huxley;
} MiniSNNConfig;

/* Criacao e destruicao */
MiniSNN *minisnn_create(int neuron_count);

MiniSNNConfig minisnn_default_config(void);

MiniSNNAdExConfig minisnn_adex_config_default(void);

MiniSNNHodgkinHuxleyConfig minisnn_hodgkin_huxley_config_default(void);

MiniSNN *minisnn_create_with_config(
    const MiniSNNConfig *config);

/*
 * Libera a rede e define o ponteiro do usuario como NULL.
 * Deve ser seguro chamar minisnn_destroy(NULL) e repetir
 * minisnn_destroy(&net) depois de net ja ter virado NULL.
 */
void minisnn_destroy(MiniSNN **snn_ptr);

/* Informacoes gerais */
int minisnn_neuron_count(const MiniSNN *snn);
int minisnn_current_step(const MiniSNN *snn);
MiniSNNNeuronModel minisnn_neuron_model(const MiniSNN *snn);
const char *minisnn_neuron_model_name(MiniSNNNeuronModel model);
MiniSNNNeuronModelCapabilities minisnn_neuron_model_capabilities(
    MiniSNNNeuronModel model);
unsigned long long minisnn_neuron_model_config_signature(
    const MiniSNN *snn);
unsigned long long minisnn_config_neuron_model_signature(
    const MiniSNNConfig *config);
const char *minisnn_neuron_integration_method(const MiniSNN *snn);
const char *minisnn_neuron_model_integration_method(
    MiniSNNNeuronModel model);
int minisnn_get_adex_state(
    const MiniSNN *snn,
    int neuron_id,
    MiniSNNAdExState *out_state);
int minisnn_get_hodgkin_huxley_state(
    const MiniSNN *snn,
    int neuron_id,
    MiniSNNHodgkinHuxleyState *out_state);

/*
 * As conexoes sao enumeradas por source crescente e, dentro de cada source,
 * pela ordem em que foram inseridas.
 */
size_t minisnn_connection_count(const MiniSNN *snn);

int minisnn_get_connection(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNConnectionInfo *out_connection);

int minisnn_get_connection_weight(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_weight);

int minisnn_set_connection_weight(
    MiniSNN *snn,
    size_t connection_id,
    double weight);

/* Configuracao */
int minisnn_set_neuron_type(
    MiniSNN *snn,
    int neuron_id,
    MiniSNNNeuronType type);

int minisnn_connect(
    MiniSNN *snn,
    int source,
    int target,
    double weight);

int minisnn_connect_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int allow_self_connection);

int minisnn_connect_delayed(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay);

int minisnn_connect_delayed_ex(
    MiniSNN *snn,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection);

/* Plasticidade sinaptica */
MiniSNNPlasticityConfig minisnn_default_plasticity_config(void);

int minisnn_set_plasticity_config(
    MiniSNN *snn,
    const MiniSNNPlasticityConfig *config);

int minisnn_get_plasticity_config(
    const MiniSNN *snn,
    MiniSNNPlasticityConfig *out_config);

int minisnn_get_plasticity_stats(
    const MiniSNN *snn,
    MiniSNNPlasticityStats *out_stats);

int minisnn_get_plasticity_traces(
    const MiniSNN *snn,
    int neuron_id,
    double *out_pre_trace,
    double *out_post_trace);

/* Aprendizado modulado por recompensa */
MiniSNNRewardConfig minisnn_default_reward_config(void);

int minisnn_set_reward_config(
    MiniSNN *snn,
    const MiniSNNRewardConfig *config);

int minisnn_get_reward_config(
    const MiniSNN *snn,
    MiniSNNRewardConfig *out_config);

int minisnn_queue_reward(MiniSNN *snn, double value);

int minisnn_get_pending_reward(
    const MiniSNN *snn,
    double *out_value);

int minisnn_clear_pending_reward(MiniSNN *snn);

int minisnn_get_last_applied_reward(
    const MiniSNN *snn,
    double *out_value);

int minisnn_reset_reward_learning(MiniSNN *snn);

int minisnn_get_reward_stats(
    const MiniSNN *snn,
    MiniSNNRewardStats *out_stats);

int minisnn_get_connection_eligibility(
    const MiniSNN *snn,
    size_t connection_id,
    double *out_eligibility);

int minisnn_get_reward_connection_stats(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNRewardConnectionStats *out_stats);

int minisnn_reward_eligible_connection_count(
    const MiniSNN *snn,
    size_t *out_count);

/* Homeostase neural simplificada */
MiniSNNHomeostasisConfig minisnn_default_homeostasis_config(void);

int minisnn_set_homeostasis_config(
    MiniSNN *snn,
    const MiniSNNHomeostasisConfig *config);

int minisnn_get_homeostasis_config(
    const MiniSNN *snn,
    MiniSNNHomeostasisConfig *out_config);

int minisnn_reset_homeostasis(MiniSNN *snn);

int minisnn_get_homeostasis_stats(
    const MiniSNN *snn,
    MiniSNNHomeostasisStats *out_stats);

int minisnn_get_neuron_rate_trace(
    const MiniSNN *snn,
    int neuron_id,
    double *out_rate_trace);

int minisnn_get_neuron_effective_threshold(
    const MiniSNN *snn,
    int neuron_id,
    double *out_threshold);

int minisnn_get_base_threshold(
    const MiniSNN *snn,
    double *out_threshold);

int minisnn_get_inhibitory_gain(
    const MiniSNN *snn,
    double *out_gain);

int minisnn_get_initial_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);

int minisnn_get_current_incoming_exc_sum(
    const MiniSNN *snn,
    int neuron_id,
    double *out_sum);

/* Topologia adaptativa e plasticidade estrutural */
int minisnn_connection_key(
    size_t neuron_count,
    size_t source,
    size_t target,
    uint64_t *out_key);

MiniSNNStructuralPlasticityConfig
minisnn_default_structural_plasticity_config(void);

int minisnn_set_structural_plasticity_config(
    MiniSNN *snn,
    const MiniSNNStructuralPlasticityConfig *config);

int minisnn_get_structural_plasticity_config(
    const MiniSNN *snn,
    MiniSNNStructuralPlasticityConfig *out_config);

int minisnn_get_structural_stats(
    const MiniSNN *snn,
    MiniSNNStructuralStats *out_stats);

int minisnn_get_topology_signature(
    const MiniSNN *snn,
    uint64_t *out_signature);

int minisnn_validate_topology_patch(
    const MiniSNN *snn,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);

int minisnn_apply_topology_patch(
    MiniSNN *snn,
    const MiniSNNTopologyOperation *operations,
    size_t operation_count,
    MiniSNNTopologyPatchResult *result);

int minisnn_get_structural_connection_state(
    const MiniSNN *snn,
    size_t connection_id,
    MiniSNNStructuralConnectionState *out_state);

size_t minisnn_structural_event_count(const MiniSNN *snn);

int minisnn_get_structural_event(
    const MiniSNN *snn,
    size_t event_index,
    MiniSNNStructuralEvent *out_event);

int minisnn_reset_structural_plasticity(
    MiniSNN *snn,
    MiniSNNStructuralResetMode mode);

/* Entrada externa */
int minisnn_set_input(
    MiniSNN *snn,
    int neuron_id,
    double current);

int minisnn_add_input(
    MiniSNN *snn,
    int neuron_id,
    double current);

void minisnn_clear_inputs(MiniSNN *snn);

/*
 * Executa um timestep.
 * Retorna numero de spikes, ou -1 para rede invalida.
 */
int minisnn_step(MiniSNN *snn);

/* Consultas do estado mais recente */
int minisnn_get_spike(
    const MiniSNN *snn,
    int neuron_id,
    int *out_spike);

int minisnn_get_voltage(
    const MiniSNN *snn,
    int neuron_id,
    double *out_voltage);

/*
 * Retorna a corrente sinaptica efetivamente usada
 * no timestep mais recente.
 */
int minisnn_get_synaptic_current(
    const MiniSNN *snn,
    int neuron_id,
    double *out_current);

#endif

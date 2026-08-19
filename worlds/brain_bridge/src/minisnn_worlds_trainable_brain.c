#include "minisnn_worlds_brain_bridge.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define WB1_MIN_NEURONS (MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1 + \
                         MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1)
#define WB1_MAX_NEURONS UINT32_C(256)
#define WB1_PATH_MAX 480U
#define WB1_FILE_MAGIC "minisnn_worlds_trainable_brain_v1"

struct MiniSNNWorldsTrainableBrain
{
    MiniSNNWorldsBrainConfig config;
    MiniSNN *network;
    MiniSNNSensorSchema *sensor_schema;
    MiniSNNActionSchema *action_schema;
    MiniSNNAgentIOContext *agent_io;
    MiniSNNSensorEncoder *encoder;
    MiniSNNActionDecoder *decoder;
    MiniSNNAgentCycle *cycle;
    const MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsDomainAction cached_action;
    MiniSNNWorldsTrainableBrainReport cached_report;
    uint64_t cached_domain_tick;
    uint64_t last_source_tick;
    uint64_t pending_domain_tick;
    uint64_t pending_core_tick;
    uint64_t pending_episode_id;
    uint64_t save_nonce;
    MiniSNNWorldsDomainAction pending_action;
    double last_reward;
    int bound;
    int cached;
    int decided;
    int feedback_pending;
    MiniSNNWorldsTrainableBrainError last_error;
};

static void write_error(MiniSNNWorldsTrainableBrainError *out_error,
                        MiniSNNWorldsTrainableBrainError error)
{
    if (out_error != NULL)
        *out_error = error;
}

static int valid_name(const char *name)
{
    size_t index;
    if (name == NULL || name[0] == '\0')
        return 0;
    for (index = 0U; name[index] != '\0'; ++index)
        if (index >= MINISNN_WORLDS_BRAIN_NAME_MAX ||
            (unsigned char)name[index] < 0x20U || (unsigned char)name[index] > 0x7eU)
            return 0;
    return 1;
}

static int build_topology(MiniSNN *network,
                          const MiniSNNWorldsBrainConfig *config)
{
    MiniSNNTopologyFactoryConfig factory = minisnn_topology_factory_default();
    factory.kind = config->topology == MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM ?
        MINISNN_TOPOLOGY_FACTORY_RANDOM :
        (config->topology == MINISNN_WORLDS_BRAIN_TOPOLOGY_SMALL_WORLD ?
         MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD :
         MINISNN_TOPOLOGY_FACTORY_FULLY_CONNECTED);
    factory.seed = config->seed;
    factory.inhibitory_count = config->inhibitory_count;
    factory.connection_probability = config->connection_probability;
    factory.small_world_neighbors = config->small_world_neighbors;
    factory.small_world_rewire_probability = config->small_world_rewire_probability;
    factory.excitatory_weight = config->excitatory_weight;
    factory.inhibitory_weight = config->inhibitory_weight;
    factory.delay = config->connection_delay;
    factory.allow_self_connections = config->allow_self_connections;
    factory.allow_inhibitory_to_inhibitory = config->allow_inhibitory_to_inhibitory;
    /* WB1 intentionally uses only generic deterministic topology generation.
     * Sensor/action identities remain an I/O contract, never initial wiring. */
    return minisnn_topology_factory_build(network, &factory);
}

static int is_organism(const MiniSNNWorldsDomain *domain,
                       MiniSNNWorldsKernelEntityId actor,
                       MiniSNNWorldsDomainOrganismInfo *out_info)
{
    size_t index;
    MiniSNNWorldsDomainOrganismInfo info;

    if (domain == NULL || actor.value == 0U)
        return 0;
    for (index = 0U; index < minisnn_worlds_domain_organism_count(domain); ++index)
    {
        if (minisnn_worlds_domain_organism_at(domain, index, &info) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
            return 0;
        if (info.entity_id.value == actor.value)
        {
            if (out_info != NULL)
                *out_info = info;
            return 1;
        }
    }
    return 0;
}

static int configure_learning(MiniSNNWorldsTrainableBrain *brain)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    int active = brain->config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING &&
                 brain->config.plasticity_enabled != 0;
    plasticity.enabled = active;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    reward.enabled = active;
    /* The Core rejects disabling plasticity while reward is still enabled.
     * Toggle in dependency order, without replacing the network or weights. */
    if (!active)
        return minisnn_set_reward_config(brain->network, &reward) &&
               minisnn_set_plasticity_config(brain->network, &plasticity);
    return minisnn_set_plasticity_config(brain->network, &plasticity) &&
           minisnn_set_reward_config(brain->network, &reward);
}

static int create_cycle(MiniSNNWorldsTrainableBrain *brain)
{
    static const char *const sensor_names[6] =
    { "energy", "hunger", "food_present", "food_dx", "food_dy", "food_distance" };
    static const char *const action_names[6] =
    { "wait", "move_pos_x", "move_neg_x", "move_pos_y", "move_neg_y", "eat" };
    MiniSNNSensorChannelSpec sensor_specs[6];
    MiniSNNActionChannelSpec action_specs[6];
    MiniSNNSensorEncodingSpec encoder_specs[6];
    MiniSNNActionDecodingSpec decoder_specs[6];
    uint32_t index;

    for (index = 0U; index < 6U; ++index)
    {
        sensor_specs[index] = (MiniSNNSensorChannelSpec)
        { 100U + index, sensor_names[index], -1.0, 1.0, 0.0 };
        action_specs[index] = (MiniSNNActionChannelSpec)
        { 200U + index, action_names[index], 0.0, 1.0, 0.0 };
        encoder_specs[index] = (MiniSNNSensorEncodingSpec)
        { 100U + index, index, 1U, MINISNN_SENSOR_ENCODING_BIPOLAR_CURRENT,
          5000.0, 0.0, 0.0, 1.0, 0U };
        memset(&decoder_specs[index], 0, sizeof(decoder_specs[index]));
        decoder_specs[index].action_channel_id = 200U + index;
        decoder_specs[index].mode = MINISNN_ACTION_DECODING_WTA_MEMBER;
        decoder_specs[index].primary_neuron_start = 6U +
            index * brain->config.action_population_size;
        decoder_specs[index].primary_neuron_count =
            brain->config.action_population_size;
        decoder_specs[index].minimum_rate = 0.0;
        decoder_specs[index].maximum_rate = 1.0;
        decoder_specs[index].competition_group_id = 1U;
        decoder_specs[index].winner_value = 1.0;
        decoder_specs[index].loser_value = 0.0;
    }
    brain->sensor_schema = minisnn_sensor_schema_create(sensor_specs, 6U, NULL);
    brain->action_schema = minisnn_action_schema_create(action_specs, 6U, NULL);
    brain->agent_io = minisnn_agent_io_create(brain->sensor_schema, brain->action_schema, NULL);
    brain->encoder = minisnn_sensor_encoder_create(brain->sensor_schema, encoder_specs, 6U,
        brain->config.neuron_count, brain->config.decision_steps_per_tick, NULL);
    brain->decoder = minisnn_action_decoder_create(brain->action_schema, decoder_specs, 6U,
        brain->config.neuron_count, brain->config.decision_steps_per_tick, NULL);
    if (brain->sensor_schema == NULL || brain->action_schema == NULL ||
        brain->agent_io == NULL || brain->encoder == NULL || brain->decoder == NULL)
        return 0;
    brain->cycle = minisnn_agent_cycle_create(brain->network, brain->agent_io,
                                               brain->encoder, brain->decoder, NULL);
    return brain->cycle != NULL;
}

MiniSNNWorldsBrainConfig minisnn_worlds_trainable_brain_config_default(void)
{
    MiniSNNWorldsBrainConfig config;
    memset(&config, 0, sizeof(config));
    config.version = MINISNN_WORLDS_BRAIN_CONFIG_VERSION_V1;
    (void)snprintf(config.brain_name, sizeof(config.brain_name), "Trainable Brain V1");
    config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_SMALL_WORLD;
    config.neuron_count = 16U;
    config.inhibitory_count = 4U;
    config.connection_probability = 0.25;
    config.small_world_neighbors = 4U;
    config.small_world_rewire_probability = 1.0 / 11.0;
    config.excitatory_weight = 100.0;
    config.inhibitory_weight = -75.0;
    config.connection_delay = 1U;
    config.allow_self_connections = 0;
    config.allow_inhibitory_to_inhibitory = 1;
    config.seed = UINT64_C(0x5742315F42524149);
    config.decision_steps_per_tick = 8U;
    config.action_population_size = 1U;
    config.mode = MINISNN_WORLDS_BRAIN_MODE_TRAINING;
    config.plasticity_enabled = 1;
    config.reward_profile.eat_applied_reward = 1.0;
    config.reward_profile.rejected_action_penalty = -0.05;
    config.neural_config = minisnn_default_config();
    config.neural_config.neuron_count = 16;
    config.neural_config.max_synaptic_delay = 1;
    return config;
}

int minisnn_worlds_trainable_brain_config_is_valid(const MiniSNNWorldsBrainConfig *config)
{
    MiniSNNConfig neural;
    if (config == NULL || config->version != MINISNN_WORLDS_BRAIN_CONFIG_VERSION_V1 ||
        !valid_name(config->brain_name) || config->neuron_count < WB1_MIN_NEURONS ||
        config->neuron_count > WB1_MAX_NEURONS ||
        config->inhibitory_count > config->neuron_count ||
        !isfinite(config->connection_probability) ||
        config->connection_probability < 0.0 || config->connection_probability > 1.0 ||
        config->small_world_neighbors >= config->neuron_count ||
        (config->topology == MINISNN_WORLDS_BRAIN_TOPOLOGY_SMALL_WORLD &&
         (config->small_world_neighbors % 2U) != 0U) ||        !isfinite(config->small_world_rewire_probability) ||
        config->small_world_rewire_probability < 0.0 ||
        config->small_world_rewire_probability > 1.0 ||
        !isfinite(config->excitatory_weight) || config->excitatory_weight <= 0.0 ||
        !isfinite(config->inhibitory_weight) || config->inhibitory_weight >= 0.0 ||
        config->connection_delay != 1U ||
        (config->allow_self_connections != 0 && config->allow_self_connections != 1) ||
        (config->allow_inhibitory_to_inhibitory != 0 &&
         config->allow_inhibitory_to_inhibitory != 1) ||
        config->decision_steps_per_tick == 0U ||
        config->decision_steps_per_tick > MINISNN_WORLDS_BRAIN_BRIDGE_MAX_CORE_STEPS ||
        config->action_population_size == 0U ||
        config->action_population_size >
            (WB1_MAX_NEURONS - MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1) /
                MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1 ||
        config->neuron_count < MINISNN_WORLDS_BRAIN_BRIDGE_SENSOR_COUNT_V1 +
            config->action_population_size *
                MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1 ||
        config->topology > MINISNN_WORLDS_BRAIN_TOPOLOGY_FULLY_CONNECTED ||
        config->mode > MINISNN_WORLDS_BRAIN_MODE_EVALUATION ||
        (config->plasticity_enabled != 0 && config->plasticity_enabled != 1) ||
        !isfinite(config->reward_profile.eat_applied_reward) ||
        !isfinite(config->reward_profile.rejected_action_penalty) ||
        config->reward_profile.eat_applied_reward < 0.0 ||
        config->reward_profile.rejected_action_penalty > 0.0)
        return 0;
    /* WB1 V1 has an explicit canonical LIF-only neural configuration. This
     * keeps save/load semantic rather than silently discarding model fields. */
    neural = config->neural_config;
    if (neural.neuron_model != MINISNN_NEURON_MODEL_LIF ||
        neural.neuron_count != (int)config->neuron_count ||
        neural.max_synaptic_delay != 1)
    {
        return 0;
    }
    return minisnn_config_is_valid(&neural);
}

const char *minisnn_worlds_brain_topology_name(MiniSNNWorldsBrainTopology topology)
{
    static const char *const names[] = { "random", "small_world", "fully_connected" };
    return topology <= MINISNN_WORLDS_BRAIN_TOPOLOGY_FULLY_CONNECTED ? names[topology] : "invalid";
}

const char *minisnn_worlds_brain_mode_name(MiniSNNWorldsBrainMode mode)
{
    return mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING ? "TRAINING" :
           mode == MINISNN_WORLDS_BRAIN_MODE_EVALUATION ? "EVALUATION" : "INVALID";
}

MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_create(
    const MiniSNNWorldsBrainConfig *config, MiniSNNWorldsTrainableBrainError *out_error)
{
    MiniSNNWorldsTrainableBrain *brain;
    MiniSNNConfig neural;
    if (!minisnn_worlds_trainable_brain_config_is_valid(config))
    { write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT); return NULL; }
    brain = calloc(1U, sizeof(*brain));
    if (brain == NULL)
    { write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_ALLOCATION); return NULL; }
    brain->config = *config;
    neural = config->neural_config;
    brain->config.neural_config = neural;
    brain->network = minisnn_create_with_config(&neural);
    if (brain->network == NULL || !build_topology(brain->network, config) || !configure_learning(brain) || !create_cycle(brain))
    { minisnn_worlds_trainable_brain_destroy(&brain); write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE); return NULL; }
    write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    return brain;
}

void minisnn_worlds_trainable_brain_destroy(MiniSNNWorldsTrainableBrain **brain_ptr)
{
    MiniSNNWorldsTrainableBrain *brain;
    if (brain_ptr == NULL || *brain_ptr == NULL) return;
    brain = *brain_ptr;
    minisnn_agent_cycle_destroy(&brain->cycle);
    minisnn_action_decoder_destroy(&brain->decoder);
    minisnn_sensor_encoder_destroy(&brain->encoder);
    minisnn_agent_io_destroy(&brain->agent_io);
    minisnn_action_schema_destroy(&brain->action_schema);
    minisnn_sensor_schema_destroy(&brain->sensor_schema);
    minisnn_destroy(&brain->network);
    free(brain);
    *brain_ptr = NULL;
}

MiniSNNWorldsTrainableBrainError minisnn_worlds_trainable_brain_last_error(const MiniSNNWorldsTrainableBrain *brain)
{ return brain == NULL ? MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT : brain->last_error; }

const char *minisnn_worlds_trainable_brain_error_string(MiniSNNWorldsTrainableBrainError error)
{
    static const char *const names[] = { "ok", "argumento invalido", "falha de alocacao", "domain incompativel", "ator desconhecido", "ator nao e organismo", "falha do Core", "erro de E/S", "formato invalido", "configuracao incompativel", "feedback nao pendente", "feedback incompativel", "modo invalido", "acao ou feedback pendente" };
    return error <= MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT ? names[error] : "erro desconhecido";
}

int minisnn_worlds_trainable_brain_get_config(const MiniSNNWorldsTrainableBrain *brain, MiniSNNWorldsBrainConfig *out_config)
{ if (brain == NULL || out_config == NULL) return 0; *out_config = brain->config; return 1; }

static int same_action(const MiniSNNWorldsDomainAction *left,
                       const MiniSNNWorldsDomainAction *right)
{
    return left != NULL && right != NULL &&
           left->actor.value == right->actor.value &&
           left->type == right->type &&
           left->move_delta.x == right->move_delta.x &&
           left->move_delta.y == right->move_delta.y &&
           left->eat_target.value == right->eat_target.value;
}

static void clear_episode_binding(MiniSNNWorldsTrainableBrain *brain)
{
    brain->domain = NULL;
    brain->actor.value = 0U;
    brain->bound = 0;
    brain->cached = 0;
    brain->decided = 0;
    brain->feedback_pending = 0;
    brain->cached_domain_tick = 0U;
    brain->last_source_tick = 0U;
    brain->pending_domain_tick = 0U;
    brain->pending_core_tick = 0U;
    brain->pending_episode_id = 0U;
    memset(&brain->cached_action, 0, sizeof(brain->cached_action));
    memset(&brain->pending_action, 0, sizeof(brain->pending_action));
    memset(&brain->cached_report, 0, sizeof(brain->cached_report));
}

int minisnn_worlds_trainable_brain_bind(MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsKernelEntityId actor)
{
    if (brain == NULL || domain == NULL)
        return 0;
    if (brain->bound)
    {
        brain->last_error = brain->domain == domain &&
            brain->actor.value == actor.value ?
            MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT :
            MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_DOMAIN_MISMATCH;
        return 0;
    }
    if (!is_organism(domain, actor, NULL))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_UNKNOWN_ACTOR;
        return 0;
    }
    brain->domain = domain;
    brain->actor = actor;
    brain->bound = 1;
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_set_mode(
    MiniSNNWorldsTrainableBrain *brain, MiniSNNWorldsBrainMode mode)
{
    MiniSNNWorldsBrainMode previous_mode;
    uint32_t feedback_count = 0U;
    double delivered_reward = 0.0;

    if (brain == NULL || mode > MINISNN_WORLDS_BRAIN_MODE_EVALUATION)
    {
        if (brain != NULL)
            brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_MODE;
        return 0;
    }
    if (brain->feedback_pending)
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT;
        return 0;
    }
    previous_mode = brain->config.mode;
    /* A reported consequence belongs to Training even when the caller now
     * requests Evaluation. Deliver it at this completed boundary, without a
     * new neural tick, before reward/plasticity are disabled. */
    if (previous_mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING &&
        mode == MINISNN_WORLDS_BRAIN_MODE_EVALUATION &&
        minisnn_agent_cycle_has_pending_feedback(brain->cycle) &&
        !minisnn_agent_cycle_drain_due_feedback(
            brain->cycle, &feedback_count, &delivered_reward))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->config.mode = mode;
    if (!configure_learning(brain))
    {
        brain->config.mode = previous_mode;
        (void)configure_learning(brain);
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_set_reward_learning_rate(
    MiniSNNWorldsTrainableBrain *brain,
    double learning_rate)
{
    MiniSNNRewardConfig reward;

    if (brain == NULL || !isfinite(learning_rate) || learning_rate <= 0.0)
    {
        if (brain != NULL)
            brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (brain->feedback_pending || minisnn_agent_cycle_has_pending_feedback(brain->cycle))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT;
        return 0;
    }
    if (!minisnn_get_reward_config(brain->network, &reward))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    reward.learning_rate = learning_rate;
    if (!minisnn_set_reward_config(brain->network, &reward))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_set_reward_eligibility_tau(
    MiniSNNWorldsTrainableBrain *brain,
    double eligibility_tau)
{
    MiniSNNRewardConfig reward;

    if (brain == NULL || !isfinite(eligibility_tau) || eligibility_tau <= 0.0)
    {
        if (brain != NULL)
            brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (brain->feedback_pending || minisnn_agent_cycle_has_pending_feedback(brain->cycle))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT;
        return 0;
    }
    if (!minisnn_get_reward_config(brain->network, &reward))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    reward.eligibility_tau = eligibility_tau;
    if (!minisnn_set_reward_config(brain->network, &reward))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_decide(MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain, MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsDomainAction *out_action, MiniSNNWorldsTrainableBrainReport *out_report)
{
    MiniSNNWorldsDomainOrganismInfo organism;
    MiniSNNWorldsDomainPerception perception;
    MiniSNNWorldsBrainSensorFrameV1 encoded;
    MiniSNNSensorFrame sensor = {0};
    MiniSNNActionFrame action = {0};
    MiniSNNAgentCycleDiagnostics diagnostics;
    MiniSNNWorldsTrainableBrainReport report;
    uint32_t scores[6] = {0U};
    uint32_t index;
    uint8_t tie;
    MiniSNNWorldsBrainBridgeFallback fallback;
    int ok = 0;
    if (brain == NULL || domain == NULL || out_action == NULL || out_report == NULL) return 0;
    if (!brain->bound || brain->domain != domain || brain->actor.value != actor.value)
    { brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_DOMAIN_MISMATCH; return 0; }
    if (brain->feedback_pending &&
        brain->pending_domain_tick != minisnn_worlds_domain_tick(domain))
    { brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_MISMATCH; return 0; }
    if (brain->cached && brain->cached_domain_tick == minisnn_worlds_domain_tick(domain))
    { report = brain->cached_report; report.cache_hit = 1U; *out_action = brain->cached_action; *out_report = report; return 1; }
    if (!is_organism(domain, actor, &organism) ||
        minisnn_worlds_domain_perceive(domain, actor, &perception) != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_brain_bridge_encode_perception_v1(&perception, organism.max_energy, 1000.0, &encoded) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE ||
        !minisnn_sensor_frame_init(&sensor, 6U) ||
        !minisnn_sensor_frame_set_values(&sensor, minisnn_agent_cycle_next_global_tick(brain->cycle), encoded.values, 6U, NULL) ||
        !minisnn_agent_io_submit_sensor_frame(brain->agent_io, &sensor) ||
        !minisnn_agent_cycle_run_tick(brain->cycle, &diagnostics) ||
        !minisnn_action_frame_init(&action, 6U) ||
        !minisnn_agent_io_consume_action_frame(brain->agent_io, &action))
        goto done;
    for (index = 0U; index < 6U; ++index)
        scores[index] = action.values[index] > 0.5 ? 1U : 0U;
    memset(&report, 0, sizeof(report));
    report.sensor_frame = encoded;
    memcpy(report.output_scores, scores, sizeof(scores));
    report.core_steps_executed = brain->config.decision_steps_per_tick;
    if (minisnn_worlds_brain_bridge_decode_scores_v1(&perception, scores, actor, 1000,
        &report.action, &report.selected_channel, &tie, &fallback) != MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE)
        goto done;
    report.domain_tick = minisnn_worlds_domain_tick(domain);
    report.core_tick = diagnostics.global_tick;
    report.total_spikes = diagnostics.total_spikes;
    report.tie_break_used = tie;
    report.fallback = fallback;
    report.last_reward = brain->last_reward;
    report.plasticity_active = (uint8_t)(brain->config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING && brain->config.plasticity_enabled != 0);
    brain->cached_domain_tick = report.domain_tick;
    brain->cached_action = report.action;
    brain->cached_report = report;
    brain->last_source_tick = report.core_tick;
    brain->pending_domain_tick = report.domain_tick;
    brain->pending_core_tick = report.core_tick;
    brain->pending_episode_id = minisnn_agent_cycle_episode_id(brain->cycle);
    brain->pending_action = report.action;
    brain->feedback_pending = 1;
    brain->cached = 1;
    brain->decided = 1;
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    *out_action = report.action; *out_report = report; ok = 1;
done:
    minisnn_sensor_frame_destroy(&sensor); minisnn_action_frame_destroy(&action);
    if (!ok) brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
    return ok;
}

int minisnn_worlds_trainable_brain_apply_action_result(
    MiniSNNWorldsTrainableBrain *brain,
    const MiniSNNWorldsDomain *domain,
    MiniSNNWorldsKernelEntityId actor,
    MiniSNNWorldsTick decision_domain_tick,
    const MiniSNNWorldsDomainAction *action,
    const MiniSNNWorldsDomainActionResult *result)
{
    MiniSNNAgentFeedback feedback;
    double reward = 0.0;

    if (brain == NULL || domain == NULL || action == NULL || result == NULL)
        return 0;
    if (!brain->feedback_pending)
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_NOT_PENDING;
        return 0;
    }
    if (domain != brain->domain || actor.value != brain->actor.value ||
        decision_domain_tick != brain->pending_domain_tick ||
        brain->pending_episode_id != minisnn_agent_cycle_episode_id(brain->cycle) ||
        !same_action(action, &brain->pending_action))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_MISMATCH;
        return 0;
    }
    if (brain->config.mode == MINISNN_WORLDS_BRAIN_MODE_TRAINING &&
        brain->config.plasticity_enabled != 0)
    {
        /* Reward the observable EAT consequence only; generic energy changes
         * from the Domain are intentionally not treated as food reward. */
        if (brain->pending_action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
            result->status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
            result->energy_after > result->energy_before)
        {
            reward = brain->config.reward_profile.eat_applied_reward;
        }
        else if (result->status == MINISNN_WORLDS_DOMAIN_ACTION_REJECTED)
        {
            reward = brain->config.reward_profile.rejected_action_penalty;
        }
        if (reward != 0.0)
        {
            feedback.source_tick = brain->pending_core_tick;
            feedback.delivery_tick = minisnn_agent_cycle_next_global_tick(brain->cycle);
            feedback.reward = reward;
            feedback.episode_terminal = 0U;
            if (!minisnn_agent_cycle_submit_feedback(brain->cycle, &feedback))
            {
                brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
                return 0;
            }
        }
    }
    /* A valid zero-reward result is still consumed exactly once. */
    brain->feedback_pending = 0;
    brain->last_reward = reward;
    brain->cached_report.last_reward = reward;
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_apply_terminal_feedback(
    MiniSNNWorldsTrainableBrain *brain,
    double reward)
{
    MiniSNNAgentFeedback feedback;

    if (brain == NULL || !isfinite(reward))
    {
        if (brain != NULL)
            brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    /* The action consequence must be consumed first so terminal feedback has
     * a single, unambiguous causal source. */
    if (!brain->decided || brain->feedback_pending)
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_NOT_PENDING;
        return 0;
    }
    if (brain->config.mode != MINISNN_WORLDS_BRAIN_MODE_TRAINING ||
        brain->config.plasticity_enabled == 0)
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_MODE;
        return 0;
    }
    feedback.source_tick = brain->last_source_tick;
    feedback.delivery_tick = minisnn_agent_cycle_next_global_tick(brain->cycle);
    feedback.reward = reward;
    feedback.episode_terminal = 1U;
    if (!minisnn_agent_cycle_submit_feedback(brain->cycle, &feedback))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->last_reward = reward;
    brain->cached_report.last_reward = reward;
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

int minisnn_worlds_trainable_brain_reset_episode(MiniSNNWorldsTrainableBrain *brain)
{
    if (brain != NULL && (brain->feedback_pending ||
                           minisnn_agent_cycle_has_pending_feedback(brain->cycle)))
    {
        brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT;
        return 0;
    }
    if (brain == NULL || !minisnn_agent_cycle_reset_episode(brain->cycle))
    {
        if (brain != NULL)
            brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE;
        return 0;
    }
    brain->last_reward = 0.0;
    clear_episode_binding(brain);
    brain->last_error = MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE;
    return 1;
}

MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_full_reset(
    const MiniSNNWorldsTrainableBrain *brain, MiniSNNWorldsTrainableBrainError *out_error)
{ return brain == NULL ? (write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT), NULL) : minisnn_worlds_trainable_brain_create(&brain->config, out_error); }

static void fnv_byte(uint64_t *hash, unsigned char byte)
{
    *hash ^= (uint64_t)byte;
    *hash *= UINT64_C(1099511628211);
}

static void fnv_u64(uint64_t *hash, uint64_t value)
{
    unsigned int shift;
    for (shift = 0U; shift < 64U; shift += 8U)
        fnv_byte(hash, (unsigned char)((value >> shift) & UINT64_C(0xff)));
}

static uint64_t brain_generation(const MiniSNNWorldsTrainableBrain *brain,
                                 uint64_t save_nonce)
{
    uint64_t topology = 0U;
    uint64_t hash = UINT64_C(14695981039346656037);

    if (brain == NULL ||
        !minisnn_get_topology_signature(brain->network, &topology))
        return 0U;
    fnv_u64(&hash, minisnn_config_neuron_model_signature(&brain->config.neural_config));
    fnv_u64(&hash, topology);
    fnv_u64(&hash, minisnn_agent_io_sensor_schema_signature(brain->agent_io));
    fnv_u64(&hash, minisnn_agent_io_action_schema_signature(brain->agent_io));
    fnv_u64(&hash, minisnn_worlds_trainable_brain_weight_signature(brain));
    fnv_u64(&hash, minisnn_agent_cycle_next_global_tick(brain->cycle));
    fnv_u64(&hash, save_nonce);
    return hash;
}

static int path_exists(const char *path)
{
    struct stat status;
    return path != NULL && stat(path, &status) == 0;
}
static int checkpoint_path(char *out, size_t size, const char *filename,
                           uint64_t generation, int temporary)
{
    int written = snprintf(out, size, "%s.checkpoint.%016llx%s", filename,
                           (unsigned long long)generation,
                           temporary ? ".tmp" : "");
    return written >= 0 && (size_t)written < size;
}

static int replace_file(const char *temporary, const char *filename)
{
#ifdef _WIN32
    return MoveFileExA(temporary, filename,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, filename) == 0;
#endif
}

static int publish_directory(const char *temporary, const char *directory)
{
#ifdef _WIN32
    return MoveFileExA(temporary, directory, MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, directory) == 0;
#endif
}

static int read_value(FILE *file, const char *key, char *out, size_t out_size)
{
    char line[192];
    size_t key_size;
    size_t value_size;

    if (file == NULL || key == NULL || out == NULL || out_size == 0U ||
        fgets(line, (int)sizeof(line), file) == NULL)
        return 0;
    key_size = strlen(key);
    if (strncmp(line, key, key_size) != 0 || line[key_size] != '=')
        return 0;
    value_size = strcspn(line + key_size + 1U, "\r\n");
    if (value_size >= out_size)
        return 0;
    memcpy(out, line + key_size + 1U, value_size);
    out[value_size] = '\0';
    return 1;
}

static int parse_unsigned(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;

    if (text == NULL || out_value == NULL)
        return 0;
    value = strtoull(text, &end, 10);
    if (end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_hex(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;

    if (text == NULL || out_value == NULL)
        return 0;
    value = strtoull(text, &end, 16);
    if (end == text || *end != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

static int parse_finite_double(const char *text, double *out_value)
{
    char *end;
    double value;

    if (text == NULL || out_value == NULL)
        return 0;
    value = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(value))
        return 0;
    *out_value = value;
    return 1;
}

#ifdef MINISNN_TESTING
static int wb1_test_fail_after_checkpoint;
void minisnn_test_wb1_fail_after_checkpoint(int enabled)
{
    wb1_test_fail_after_checkpoint = enabled != 0;
}
#endif

static int write_manifest(FILE *file, const MiniSNNWorldsTrainableBrain *brain,
                          uint64_t generation)
{
    const MiniSNNConfig *neural = &brain->config.neural_config;

    return fprintf(file,
        "%s\ngeneration=%016llx\nname=%s\ntopology=%u\nneurons=%u\ninhibitory_count=%u\nconnection_probability=%.17g\nsmall_world_neighbors=%u\nsmall_world_rewire_probability=%.17g\nexcitatory_weight=%.17g\ninhibitory_weight=%.17g\nconnection_delay=%u\nallow_self_connections=%d\nallow_inhibitory_to_inhibitory=%d\nseed=%llu\nsteps=%u\nmode=%u\nplasticity=%d\neat_reward=%.17g\nrejected_reward=%.17g\nmodel=%s\ndt=%.17g\ntau=%.17g\nv_rest=%.17g\nv_reset=%.17g\nv_threshold=%.17g\nresistance=%.17g\nsynaptic_decay=%.17g\nmax_synaptic_delay=%d\nsensor_schema_signature=%016llx\naction_schema_signature=%016llx\nweight_signature=%016llx\ncore_tick=%llu\naction_population_size=%u\n",
        WB1_FILE_MAGIC, (unsigned long long)generation, brain->config.brain_name,
        (unsigned)brain->config.topology, brain->config.neuron_count,
        brain->config.inhibitory_count, brain->config.connection_probability,
        brain->config.small_world_neighbors,
        brain->config.small_world_rewire_probability,
        brain->config.excitatory_weight, brain->config.inhibitory_weight,
        brain->config.connection_delay, brain->config.allow_self_connections,
        brain->config.allow_inhibitory_to_inhibitory,
        (unsigned long long)brain->config.seed, brain->config.decision_steps_per_tick,
        (unsigned)brain->config.mode, brain->config.plasticity_enabled,
        brain->config.reward_profile.eat_applied_reward,
        brain->config.reward_profile.rejected_action_penalty,
        minisnn_neuron_model_name(neural->neuron_model), neural->dt, neural->tau,
        neural->v_rest, neural->v_reset, neural->v_threshold, neural->resistance,
        neural->synaptic_decay, neural->max_synaptic_delay,
        (unsigned long long)minisnn_agent_io_sensor_schema_signature(brain->agent_io),
        (unsigned long long)minisnn_agent_io_action_schema_signature(brain->agent_io),
        (unsigned long long)minisnn_worlds_trainable_brain_weight_signature(brain),
        (unsigned long long)minisnn_agent_cycle_next_global_tick(brain->cycle),
        brain->config.action_population_size) >= 0;
}
int minisnn_worlds_trainable_brain_save(const MiniSNNWorldsTrainableBrain *brain,
    const char *filename, MiniSNNWorldsTrainableBrainError *out_error)
{
    char checkpoint[WB1_PATH_MAX];
    char staging[WB1_PATH_MAX];
    char temporary[WB1_PATH_MAX];
    uint64_t generation;
    FILE *file;

    if (brain == NULL || filename == NULL || filename[0] == '\0')
    {
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    if (brain->feedback_pending ||
        minisnn_agent_cycle_has_pending_feedback(brain->cycle))
    {
        write_error(out_error,
                    MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT);
        return 0;
    }
    {
        uint64_t attempt;
        for (attempt = 0U; attempt < UINT64_C(1024); ++attempt)
        {
            generation = brain_generation(brain, brain->save_nonce + attempt);
            if (generation != 0U &&
                checkpoint_path(checkpoint, sizeof(checkpoint), filename, generation, 0) &&
                checkpoint_path(staging, sizeof(staging), filename, generation, 1) &&
                !path_exists(checkpoint) && !path_exists(staging))
                break;
        }
        if (attempt == UINT64_C(1024))
        {
            write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
            return 0;
        }
        ((MiniSNNWorldsTrainableBrain *)brain)->save_nonce += attempt + 1U;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", filename) < 0 ||
        !minisnn_agent_cycle_save_checkpoint(brain->cycle, staging, NULL))
    {
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
    if (!publish_directory(staging, checkpoint))
    {
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
#ifdef MINISNN_TESTING
    if (wb1_test_fail_after_checkpoint)
    {
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
#endif
    file = fopen(temporary, "wb");
    if (file == NULL || !write_manifest(file, brain, generation))
    {
        if (file != NULL)
            fclose(file);
        remove(temporary);
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
    if (fclose(file) != 0)
    {
        remove(temporary);
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
    if (!replace_file(temporary, filename))
    {
        remove(temporary);
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_IO);
        return 0;
    }
    write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    return 1;
}

MiniSNNWorldsTrainableBrain *minisnn_worlds_trainable_brain_load(
    const char *filename, MiniSNNWorldsTrainableBrainError *out_error)
{
    MiniSNNWorldsBrainConfig config = minisnn_worlds_trainable_brain_config_default();
    MiniSNNWorldsTrainableBrain *candidate = NULL;
    char magic[96];
    char value[128];
    char checkpoint[WB1_PATH_MAX];
    uint64_t integer;
    uint64_t generation;
    uint64_t sensor_signature;
    uint64_t action_signature;
    uint64_t weight_signature;
    uint64_t core_tick;
    int next_character;
    FILE *file = NULL;

    if (filename == NULL || (file = fopen(filename, "rb")) == NULL ||
        fgets(magic, (int)sizeof(magic), file) == NULL ||
        strncmp(magic, WB1_FILE_MAGIC, strlen(WB1_FILE_MAGIC)) != 0 ||
        !read_value(file, "generation", value, sizeof(value)) ||
        !parse_hex(value, &generation) ||
        !read_value(file, "name", config.brain_name, sizeof(config.brain_name)) ||
        !read_value(file, "topology", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) ||
        integer > MINISNN_WORLDS_BRAIN_TOPOLOGY_FULLY_CONNECTED)
        goto format_error;
    config.topology = (MiniSNNWorldsBrainTopology)integer;
    if (!read_value(file, "neurons", value, sizeof(value)) || !parse_unsigned(value, &integer) ||
        integer > UINT32_MAX)
        goto format_error;
    config.neuron_count = (uint32_t)integer;
    if (!read_value(file, "inhibitory_count", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > UINT32_MAX)
        goto format_error;
    config.inhibitory_count = (uint32_t)integer;
    if (!read_value(file, "connection_probability", value, sizeof(value)) ||
        !parse_finite_double(value, &config.connection_probability) ||
        !read_value(file, "small_world_neighbors", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > UINT32_MAX)
        goto format_error;
    config.small_world_neighbors = (uint32_t)integer;
    if (!read_value(file, "small_world_rewire_probability", value, sizeof(value)) ||
        !parse_finite_double(value, &config.small_world_rewire_probability) ||
        !read_value(file, "excitatory_weight", value, sizeof(value)) ||
        !parse_finite_double(value, &config.excitatory_weight) ||
        !read_value(file, "inhibitory_weight", value, sizeof(value)) ||
        !parse_finite_double(value, &config.inhibitory_weight) ||
        !read_value(file, "connection_delay", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > UINT32_MAX)
        goto format_error;
    config.connection_delay = (uint32_t)integer;
    if (!read_value(file, "allow_self_connections", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > 1U)
        goto format_error;
    config.allow_self_connections = (int)integer;
    if (!read_value(file, "allow_inhibitory_to_inhibitory", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > 1U)
        goto format_error;
    config.allow_inhibitory_to_inhibitory = (int)integer;    if (!read_value(file, "seed", value, sizeof(value)) || !parse_unsigned(value, &config.seed) ||
        !read_value(file, "steps", value, sizeof(value)) || !parse_unsigned(value, &integer) ||
        integer > UINT32_MAX)
        goto format_error;
    config.decision_steps_per_tick = (uint32_t)integer;
    if (!read_value(file, "mode", value, sizeof(value)) || !parse_unsigned(value, &integer) ||
        integer > MINISNN_WORLDS_BRAIN_MODE_EVALUATION)
        goto format_error;
    config.mode = (MiniSNNWorldsBrainMode)integer;
    if (!read_value(file, "plasticity", value, sizeof(value)) || !parse_unsigned(value, &integer) ||
        integer > 1U)
        goto format_error;
    config.plasticity_enabled = (int)integer;
    if (!read_value(file, "eat_reward", value, sizeof(value)) ||
        !parse_finite_double(value, &config.reward_profile.eat_applied_reward) ||
        !read_value(file, "rejected_reward", value, sizeof(value)) ||
        !parse_finite_double(value, &config.reward_profile.rejected_action_penalty) ||
        !read_value(file, "model", value, sizeof(value)) ||
        !minisnn_neuron_model_from_name(value, &config.neural_config.neuron_model) ||
        !read_value(file, "dt", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.dt) ||
        !read_value(file, "tau", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.tau) ||
        !read_value(file, "v_rest", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.v_rest) ||
        !read_value(file, "v_reset", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.v_reset) ||
        !read_value(file, "v_threshold", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.v_threshold) ||
        !read_value(file, "resistance", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.resistance) ||
        !read_value(file, "synaptic_decay", value, sizeof(value)) ||
        !parse_finite_double(value, &config.neural_config.synaptic_decay) ||
        !read_value(file, "max_synaptic_delay", value, sizeof(value)) ||
        !parse_unsigned(value, &integer) || integer > INT_MAX)
        goto format_error;
    config.neural_config.neuron_count = (int)config.neuron_count;
    config.neural_config.max_synaptic_delay = (int)integer;
    if (!read_value(file, "sensor_schema_signature", value, sizeof(value)) ||
        !parse_hex(value, &sensor_signature) ||
        !read_value(file, "action_schema_signature", value, sizeof(value)) ||
        !parse_hex(value, &action_signature) ||
        !read_value(file, "weight_signature", value, sizeof(value)) ||
        !parse_hex(value, &weight_signature) ||
        !read_value(file, "core_tick", value, sizeof(value)) ||
        !parse_unsigned(value, &core_tick))
        goto format_error;
    next_character = fgetc(file);
    if (next_character != EOF)
    {
        if (ungetc(next_character, file) == EOF ||
            !read_value(file, "action_population_size", value, sizeof(value)) ||
            !parse_unsigned(value, &integer) || integer > UINT32_MAX)
        {
            goto format_error;
        }
        config.action_population_size = (uint32_t)integer;
    }
    if (fclose(file) != 0 ||
        !checkpoint_path(checkpoint, sizeof(checkpoint), filename, generation, 0))
    {
        goto format_error_no_close;
    }
    file = NULL;
    candidate = minisnn_worlds_trainable_brain_create(&config, out_error);
    if (candidate == NULL)
        return NULL;
    if (sensor_signature != minisnn_agent_io_sensor_schema_signature(candidate->agent_io) ||
        action_signature != minisnn_agent_io_action_schema_signature(candidate->agent_io) ||
        !minisnn_agent_cycle_load_checkpoint(candidate->cycle, checkpoint, NULL) ||
        minisnn_worlds_trainable_brain_weight_signature(candidate) != weight_signature ||
        minisnn_agent_cycle_next_global_tick(candidate->cycle) != core_tick)
    {
        minisnn_worlds_trainable_brain_destroy(&candidate);
        write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INCOMPATIBLE);
        return NULL;
    }
    write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    return candidate;
format_error:
    if (file != NULL)
        fclose(file);
format_error_no_close:
    write_error(out_error, MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FORMAT);
    return NULL;
}
int minisnn_worlds_trainable_brain_core_step(const MiniSNNWorldsTrainableBrain *brain)
{ return brain == NULL ? -1 : minisnn_current_step(brain->network); }

uint64_t minisnn_worlds_trainable_brain_weight_signature(
    const MiniSNNWorldsTrainableBrain *brain)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    if (brain == NULL)
        return 0U;
    for (index = 0U; index < minisnn_connection_count(brain->network); ++index)
    {
        double weight;
        uint64_t bits;
        unsigned int shift;
        if (!minisnn_get_connection_weight(brain->network, index, &weight))
            return 0U;
        memcpy(&bits, &weight, sizeof(bits));
        for (shift = 0U; shift < 64U; shift += 8U)
        {
            hash ^= (bits >> shift) & UINT64_C(0xff);
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

uint64_t minisnn_worlds_trainable_brain_topology_signature(
    const MiniSNNWorldsTrainableBrain *brain)
{
    uint64_t signature = 0U;

    if (brain == NULL ||
        !minisnn_get_topology_signature(brain->network, &signature))
        return 0U;
    return signature;
}

static void fnv_double(uint64_t *hash, double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    fnv_u64(hash, bits);
}

uint64_t minisnn_worlds_trainable_brain_config_signature(
    const MiniSNNWorldsTrainableBrain *brain)
{
    const MiniSNNWorldsBrainConfig *config;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    if (brain == NULL)
        return 0U;
    config = &brain->config;
    fnv_u64(&hash, config->version);
    for (index = 0U; config->brain_name[index] != '\0'; ++index)
        fnv_byte(&hash, (unsigned char)config->brain_name[index]);
    fnv_byte(&hash, 0U);
    fnv_u64(&hash, config->topology);
    fnv_u64(&hash, config->neuron_count);
    fnv_u64(&hash, config->inhibitory_count);
    fnv_double(&hash, config->connection_probability);
    fnv_u64(&hash, config->small_world_neighbors);
    fnv_double(&hash, config->small_world_rewire_probability);
    fnv_double(&hash, config->excitatory_weight);
    fnv_double(&hash, config->inhibitory_weight);
    fnv_u64(&hash, config->connection_delay);
    fnv_u64(&hash, (uint64_t)config->allow_self_connections);
    fnv_u64(&hash, (uint64_t)config->allow_inhibitory_to_inhibitory);
    fnv_u64(&hash, config->seed);
    fnv_u64(&hash, config->decision_steps_per_tick);
    fnv_u64(&hash, config->action_population_size);
    fnv_u64(&hash, config->mode);
    fnv_u64(&hash, (uint64_t)config->plasticity_enabled);
    fnv_double(&hash, config->reward_profile.eat_applied_reward);
    fnv_double(&hash, config->reward_profile.rejected_action_penalty);
    fnv_u64(&hash, minisnn_config_neuron_model_signature(&config->neural_config));
    return hash;
}
double minisnn_worlds_trainable_brain_last_reward(const MiniSNNWorldsTrainableBrain *brain)
{ return brain == NULL ? 0.0 : brain->last_reward; }

int minisnn_worlds_trainable_brain_get_reward_stats(
    const MiniSNNWorldsTrainableBrain *brain,
    MiniSNNRewardStats *out_stats)
{
    return brain != NULL && minisnn_get_reward_stats(brain->network, out_stats);
}

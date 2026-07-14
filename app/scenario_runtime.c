#include "scenario_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static void minisnn_config_from_scenario(
    const ScenarioConfig *config,
    MiniSNNConfig *out_config)
{
    out_config->neuron_count = config->neurons;
    out_config->dt = config->dt;
    out_config->tau = config->tau;
    out_config->v_rest = config->v_rest;
    out_config->v_reset = config->v_reset;
    out_config->v_threshold = config->v_threshold;
    out_config->resistance = config->resistance;
    out_config->synaptic_decay = config->synaptic_decay;
    out_config->max_synaptic_delay = config->max_synaptic_delay;
}

int scenario_runtime_inhibitory_count(const ScenarioConfig *config)
{
    int count;

    if (config == NULL)
        return 0;

    count = (int)((double)config->neurons *
                  config->inhibitory_fraction + 0.5);
    if (count < 0)
        return 0;
    if (count > config->neurons)
        return config->neurons;
    return count;
}

int scenario_runtime_neuron_is_inhibitory(
    int neuron_id,
    int neuron_count,
    int inhibitory_count)
{
    return neuron_id >= neuron_count - inhibitory_count;
}

const char *scenario_runtime_type_name(
    int neuron_id,
    int neuron_count,
    int inhibitory_count)
{
    return scenario_runtime_neuron_is_inhibitory(
               neuron_id,
               neuron_count,
               inhibitory_count) ?
        "INH" : "EXC";
}

void scenario_blueprint_destroy(ScenarioBlueprint *blueprint)
{
    if (blueprint == NULL)
        return;

    free(blueprint->neuron_types);
    free(blueprint->connections);
    memset(blueprint, 0, sizeof(*blueprint));
}

int scenario_runtime_capture_network(
    const MiniSNN *snn,
    int inhibitory_count,
    unsigned long long topology_signature,
    ScenarioBlueprint *out_blueprint,
    char *error_message,
    size_t error_message_size)
{
    ScenarioBlueprint blueprint;
    int neuron_count;

    if (snn == NULL || out_blueprint == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo ao capturar blueprint");
        return 0;
    }

    memset(&blueprint, 0, sizeof(blueprint));
    neuron_count = minisnn_neuron_count(snn);
    if (neuron_count <= 0 || neuron_count > SCENARIO_RUNTIME_MAX_NEURONS ||
        inhibitory_count < 0 || inhibitory_count > neuron_count)
    {
        set_error(error_message, error_message_size, "rede invalida ao capturar blueprint");
        return 0;
    }

    blueprint.neuron_count = neuron_count;
    blueprint.inhibitory_count = inhibitory_count;
    blueprint.connection_count = minisnn_connection_count(snn);
    blueprint.topology_signature = topology_signature;
    blueprint.neuron_types = malloc(
        (size_t)neuron_count * sizeof(*blueprint.neuron_types));

    if (blueprint.connection_count > 0)
    {
        blueprint.connections = malloc(
            blueprint.connection_count * sizeof(*blueprint.connections));
    }

    if (blueprint.neuron_types == NULL ||
        (blueprint.connection_count > 0 && blueprint.connections == NULL))
    {
        scenario_blueprint_destroy(&blueprint);
        set_error(error_message, error_message_size, "memoria insuficiente para blueprint");
        return 0;
    }

    for (int neuron_id = 0; neuron_id < neuron_count; neuron_id++)
    {
        blueprint.neuron_types[neuron_id] =
            scenario_runtime_neuron_is_inhibitory(
                neuron_id,
                neuron_count,
                inhibitory_count) ?
                MINISNN_NEURON_INHIBITORY : MINISNN_NEURON_EXCITATORY;
    }

    for (size_t connection_id = 0;
         connection_id < blueprint.connection_count;
         connection_id++)
    {
        if (!minisnn_get_connection(
                snn,
                connection_id,
                &blueprint.connections[connection_id]))
        {
            scenario_blueprint_destroy(&blueprint);
            set_error(error_message, error_message_size, "erro ao ler conexao do blueprint");
            return 0;
        }
    }

    scenario_blueprint_destroy(out_blueprint);
    *out_blueprint = blueprint;
    return 1;
}

int scenario_runtime_create_from_blueprint(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    MiniSNN **out_snn,
    char *error_message,
    size_t error_message_size)
{
    MiniSNNConfig minisnn_config;
    MiniSNN *snn;

    if (config == NULL || blueprint == NULL || out_snn == NULL ||
        blueprint->neuron_types == NULL ||
        blueprint->neuron_count != config->neurons ||
        (blueprint->connection_count > 0 && blueprint->connections == NULL))
    {
        set_error(error_message, error_message_size, "blueprint invalido");
        return 0;
    }

    *out_snn = NULL;
    minisnn_config_from_scenario(config, &minisnn_config);
    snn = minisnn_create_with_config(&minisnn_config);
    if (snn == NULL)
    {
        set_error(error_message, error_message_size, "erro ao criar rede do blueprint");
        return 0;
    }

    for (int neuron_id = 0; neuron_id < blueprint->neuron_count; neuron_id++)
    {
        if (!minisnn_set_neuron_type(
                snn,
                neuron_id,
                blueprint->neuron_types[neuron_id]))
        {
            minisnn_destroy(&snn);
            set_error(error_message, error_message_size, "tipo neuronal invalido no blueprint");
            return 0;
        }
    }

    for (size_t connection_id = 0;
         connection_id < blueprint->connection_count;
         connection_id++)
    {
        const MiniSNNConnectionInfo *connection =
            &blueprint->connections[connection_id];
        int allow_self = connection->source == connection->target;

        if (!minisnn_connect_delayed_ex(
                snn,
                (int)connection->source,
                (int)connection->target,
                connection->weight,
                (int)connection->delay,
                allow_self))
        {
            minisnn_destroy(&snn);
            set_error(error_message, error_message_size, "erro ao reconstruir conexao do blueprint");
            return 0;
        }
    }

    *out_snn = snn;
    return 1;
}

int scenario_runtime_configure_modules(
    MiniSNN *snn,
    const ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    MiniSNNPlasticityConfig plasticity;
    MiniSNNRewardConfig reward;
    MiniSNNHomeostasisConfig homeostasis;

    if (snn == NULL || config == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo ao configurar runtime");
        return 0;
    }

    plasticity = minisnn_default_plasticity_config();
    plasticity.enabled = config->plasticity_enabled;
    plasticity.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
    plasticity.learning_mode = strcmp(
        config->plasticity_learning_mode,
        "reward_modulated_stdp") == 0 ?
        MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP :
        MINISNN_LEARNING_MODE_DIRECT_STDP;
    plasticity.a_plus = config->plasticity_a_plus;
    plasticity.a_minus = config->plasticity_a_minus;
    plasticity.tau_plus = config->plasticity_tau_plus;
    plasticity.tau_minus = config->plasticity_tau_minus;
    plasticity.trace_increment = config->plasticity_trace_increment;
    plasticity.weight_min = config->plasticity_weight_min;
    plasticity.weight_max = config->plasticity_weight_max;
    if (!minisnn_set_plasticity_config(snn, &plasticity))
    {
        set_error(error_message, error_message_size, "erro ao configurar plasticidade");
        return 0;
    }

    reward = minisnn_default_reward_config();
    reward.enabled = config->reward_enabled;
    reward.mode = MINISNN_REWARD_MODE_RSTDP;
    reward.learning_rate = config->reward_learning_rate;
    reward.eligibility_tau = config->reward_eligibility_tau;
    reward.eligibility_min = config->reward_eligibility_min;
    reward.eligibility_max = config->reward_eligibility_max;
    reward.reward_min = config->reward_min;
    reward.reward_max = config->reward_max;
    reward.clip_reward = config->reward_clip;
    if (!minisnn_set_reward_config(snn, &reward))
    {
        set_error(error_message, error_message_size, "erro ao configurar reward");
        return 0;
    }

    homeostasis = minisnn_default_homeostasis_config();
    homeostasis.enabled = config->homeostasis_enabled;
    homeostasis.intrinsic_enabled = config->homeostasis_intrinsic_enabled;
    homeostasis.target_rate = config->homeostasis_target_rate;
    homeostasis.rate_tau = config->homeostasis_rate_tau;
    homeostasis.update_interval_steps =
        (unsigned int)config->homeostasis_update_interval_steps;
    homeostasis.threshold_eta = config->homeostasis_threshold_eta;
    homeostasis.threshold_min = config->homeostasis_threshold_min;
    homeostasis.threshold_max = config->homeostasis_threshold_max;
    homeostasis.synaptic_scaling_enabled =
        config->homeostasis_synaptic_scaling_enabled;
    homeostasis.scaling_eta = config->homeostasis_scaling_eta;
    homeostasis.scaling_min_factor = config->homeostasis_scaling_min_factor;
    homeostasis.scaling_max_factor = config->homeostasis_scaling_max_factor;
    homeostasis.scaling_weight_min = config->homeostasis_scaling_weight_min;
    homeostasis.scaling_weight_max = config->homeostasis_scaling_weight_max;
    homeostasis.inhibitory_gain_enabled =
        config->homeostasis_inhibitory_gain_enabled;
    homeostasis.inhibitory_gain_initial =
        config->homeostasis_inhibitory_gain_initial;
    homeostasis.inhibitory_gain_eta = config->homeostasis_inhibitory_gain_eta;
    homeostasis.inhibitory_gain_min = config->homeostasis_inhibitory_gain_min;
    homeostasis.inhibitory_gain_max = config->homeostasis_inhibitory_gain_max;
    if (!minisnn_set_homeostasis_config(snn, &homeostasis))
    {
        set_error(error_message, error_message_size, "erro ao configurar homeostase");
        return 0;
    }

    return 1;
}

int scenario_runtime_step(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int step,
    ScenarioRuntimeStep *out_step,
    char *error_message,
    size_t error_message_size)
{
    int reported_spikes;

    if (snn == NULL || config == NULL || out_step == NULL || step < 0 ||
        config->neurons <= 0 || config->neurons > SCENARIO_RUNTIME_MAX_NEURONS ||
        inhibitory_count < 0 || inhibitory_count > config->neurons)
    {
        set_error(error_message, error_message_size, "argumento invalido no passo do runtime");
        return 0;
    }

    memset(out_step, 0, sizeof(*out_step));
    out_step->step = step;

    if (config->reward_enabled)
    {
        for (int i = 0; i < config->reward_event_count; i++)
        {
            if (config->reward_events[i].step == step)
            {
                out_step->scheduled_reward += config->reward_events[i].value;
                out_step->reward_component_count++;
            }
        }

        if (!isfinite(out_step->scheduled_reward) ||
            (out_step->reward_component_count > 0 &&
             !minisnn_queue_reward(snn, out_step->scheduled_reward)))
        {
            set_error(error_message, error_message_size, "erro ao agendar reward");
            return 0;
        }
    }

    minisnn_clear_inputs(snn);
    for (int source = 0; source < config->source_count; source++)
    {
        if (!minisnn_set_input(snn, source, config->input_current))
        {
            set_error(error_message, error_message_size, "erro ao aplicar entrada externa");
            return 0;
        }
    }

    reported_spikes = minisnn_step(snn);
    if (reported_spikes < 0)
    {
        set_error(error_message, error_message_size, "minisnn_step falhou");
        return 0;
    }

    for (int neuron_id = 0; neuron_id < config->neurons; neuron_id++)
    {
        int spike;
        double voltage;
        double synaptic_current;

        if (!minisnn_get_spike(snn, neuron_id, &spike) ||
            !minisnn_get_voltage(snn, neuron_id, &voltage) ||
            !minisnn_get_synaptic_current(snn, neuron_id, &synaptic_current) ||
            (spike != 0 && spike != 1) ||
            !isfinite(voltage) || !isfinite(synaptic_current))
        {
            set_error(error_message, error_message_size, "estado neural invalido");
            return 0;
        }

        out_step->spikes[neuron_id] = spike;
        out_step->voltages[neuron_id] = voltage;
        out_step->synaptic_currents[neuron_id] = synaptic_current;
        out_step->voltage_sum += voltage;
        out_step->synaptic_current_sum += synaptic_current;

        if (spike)
        {
            out_step->spikes_total++;
            if (scenario_runtime_neuron_is_inhibitory(
                    neuron_id,
                    config->neurons,
                    inhibitory_count))
                out_step->spikes_inh++;
            else
                out_step->spikes_exc++;
        }
    }

    if (reported_spikes != out_step->spikes_total)
    {
        set_error(error_message, error_message_size, "contagem de spikes inconsistente");
        return 0;
    }

    return 1;
}

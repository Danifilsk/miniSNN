#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "config.h"

static void network_reset_fields(Network *net)
{
    if (net == NULL)
        return;

    net->neurons = NULL;
    net->step_snapshot = NULL;
    net->connections = NULL;
    net->spikes = NULL;
    net->syn_current = NULL;
    net->used_syn_current = NULL;
    net->pending_current = NULL;
    net->ext_current = NULL;
    net->plasticity = NULL;
    net->homeostasis = NULL;
    net->reward = NULL;
    net->structural_plasticity = NULL;

    memset(&net->model_config, 0, sizeof(net->model_config));
    net->synaptic_decay = 0.0;

    net->size = 0;
    net->step = 0;
    net->max_synaptic_delay = 0;
    net->delay_cursor = 0;
}

static int network_is_valid_for_update(Network *net)
{
    if (net == NULL ||
        net->size <= 0 ||
        net->max_synaptic_delay <= 0)
    {
        return 0;
    }

    if (!neuron_model_validate_config(&net->model_config) ||
        !isfinite(net->synaptic_decay) ||
        net->synaptic_decay < 0.0 ||
        net->synaptic_decay > 1.0)
    {
        return 0;
    }

    if (net->neurons == NULL ||
        net->step_snapshot == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL ||
        net->pending_current == NULL ||
        net->ext_current == NULL ||
        net->plasticity == NULL ||
        net->homeostasis == NULL ||
        net->reward == NULL)
    {
        return 0;
    }

    return 1;
}

int network_init(Network *net, int size)
{
    NetworkConfig config;

    network_config_default(&config);
    return network_init_with_config(net, size, &config);
}

void network_config_default(NetworkConfig *out_config)
{
    if (out_config == NULL)
        return;

    lif_parameters_default(&out_config->lif);
    adex_parameters_default(&out_config->adex);
    hodgkin_huxley_parameters_default(&out_config->hodgkin_huxley);
    out_config->neuron_model = MINISNN_NEURON_MODEL_LIF;
    out_config->synaptic_decay = SYN_DECAY;
    out_config->max_synaptic_delay = MAX_SYNAPTIC_DELAY;
}

int network_config_is_valid(
    const NetworkConfig *config)
{
    if (config == NULL)
        return 0;

    {
        NeuronModelConfig model_config;
        if (config->neuron_model == MINISNN_NEURON_MODEL_LIF)
            neuron_model_config_lif(&model_config, &config->lif);
        else if (config->neuron_model == MINISNN_NEURON_MODEL_ADEX)
            neuron_model_config_adex(&model_config, &config->adex);
        else if (config->neuron_model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
            neuron_model_config_hodgkin_huxley(
                &model_config, &config->hodgkin_huxley);
        else
            return 0;
        if (!neuron_model_validate_config(&model_config))
            return 0;
    }

    if (!isfinite(config->synaptic_decay) ||
        config->synaptic_decay < 0.0 ||
        config->synaptic_decay > 1.0)
    {
        return 0;
    }

    if (config->max_synaptic_delay < 1)
        return 0;

    return 1;
}

int network_init_with_config(
    Network *net,
    int size,
    const NetworkConfig *config)
{
    if (net == NULL)
        return 0;

    network_reset_fields(net);

    if (size <= 0 || !network_config_is_valid(config))
        return 0;

    net->size = size;
    net->step = 0;
    if (config->neuron_model == MINISNN_NEURON_MODEL_LIF)
        neuron_model_config_lif(&net->model_config, &config->lif);
    else if (config->neuron_model == MINISNN_NEURON_MODEL_ADEX)
        neuron_model_config_adex(&net->model_config, &config->adex);
    else
        neuron_model_config_hodgkin_huxley(
            &net->model_config, &config->hodgkin_huxley);
    net->synaptic_decay = config->synaptic_decay;
    net->max_synaptic_delay = config->max_synaptic_delay;
    net->delay_cursor = 0;

    net->neurons = malloc(size * sizeof(Neuron));
    net->step_snapshot = malloc(size * sizeof(Neuron));
    net->connections = malloc(size * sizeof(ConnectionList));

    if (net->connections != NULL)
    {
        for (int i = 0; i < size; i++)
        {
            net->connections[i].list = NULL;
            net->connections[i].count = 0;
        }
    }

    net->spikes = malloc(size * sizeof(int));

    // Corrente sináptica do passo atual
    net->syn_current = malloc(size * sizeof(double));
    net->used_syn_current = malloc(size * sizeof(double));

    net->pending_current =
        malloc((size_t)size *
               (size_t)net->max_synaptic_delay *
               sizeof(double));

    // Corrente externa
    net->ext_current = malloc(size * sizeof(double));
    net->plasticity = calloc(1, sizeof(*net->plasticity));
    net->homeostasis = calloc(1, sizeof(*net->homeostasis));
    net->reward = calloc(1, sizeof(*net->reward));

    if (net->neurons == NULL ||
        net->step_snapshot == NULL ||
        net->connections == NULL ||
        net->spikes == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL ||
        net->pending_current == NULL ||
        net->ext_current == NULL ||
        net->plasticity == NULL ||
        net->homeostasis == NULL ||
        net->reward == NULL)
    {
        network_destroy(net);
        return 0;
    }

    if (!plasticity_state_init(net->plasticity, size))
    {
        network_destroy(net);
        return 0;
    }

    if (!homeostasis_state_init(net->homeostasis, size))
    {
        network_destroy(net);
        return 0;
    }

    if (!reward_state_init(net->reward))
    {
        network_destroy(net);
        return 0;
    }

    for (int i = 0; i < size; i++)
    {
        if (!neuron_model_init(&net->neurons[i], &net->model_config))
        {
            network_destroy(net);
            return 0;
        }

        net->spikes[i] = 0;

        net->syn_current[i] = 0.0;
        net->used_syn_current[i] = 0.0;
        net->ext_current[i] = 0.0;

        net->connections[i].list = NULL;
        net->connections[i].count = 0;
    }

    for (int i = 0; i < size * net->max_synaptic_delay; i++)
        net->pending_current[i] = 0.0;

    return 1;
}

int network_update(Network *net)
{
    int total_spikes = 0;
    int next_slot;
    int homeostasis_enabled;
    int reward_enabled;
    MiniSNNNeuronModelCapabilities capabilities;

    if (!network_is_valid_for_update(net))
        return -1;

    homeostasis_enabled = net->homeostasis->config.enabled;
    reward_enabled = net->reward->config.enabled;
    capabilities = neuron_model_capabilities(net->model_config.model);

    if (!homeostasis_validate_capabilities(
            &net->homeostasis->config, &capabilities))
    {
        return -1;
    }

    if ((reward_enabled &&
         (!net->plasticity->config.enabled ||
          net->plasticity->config.learning_mode !=
              MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP)) ||
        (!reward_enabled && net->plasticity->config.enabled &&
         net->plasticity->config.learning_mode ==
             MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP))
    {
        return -1;
    }

    if (reward_enabled &&
        !reward_state_ensure(
            net->reward,
            net->neurons,
            net->connections,
            net->plasticity))
    {
        return -1;
    }

    // Limpa os spikes do passo anterior
    for (int i = 0; i < net->size; i++)
        net->spikes[i] = 0;

    memcpy(
        net->step_snapshot,
        net->neurons,
        (size_t)net->size * sizeof(*net->neurons));

    // Corrente total = externa + sináptica
    for (int i = 0; i < net->size; i++)
    {
        net->used_syn_current[i] = net->syn_current[i];

        double I = net->ext_current[i] + net->used_syn_current[i];

        NeuronStepContext context = {
            I,
            homeostasis_enabled && net->homeostasis->config.intrinsic_enabled,
            (homeostasis_enabled && net->homeostasis->config.intrinsic_enabled) ?
                net->homeostasis->effective_threshold[i] : 0.0
        };
        net->spikes[i] = neuron_model_step(
            &net->neurons[i], &net->model_config, &context);

        if (net->spikes[i] < 0)
        {
            memcpy(
                net->neurons,
                net->step_snapshot,
                (size_t)net->size * sizeof(*net->neurons));
            for (int j = 0; j < net->size; j++)
                net->spikes[j] = 0;
            net->spikes[i] = 0;
            return -1;
        }

        if (net->spikes[i])
            total_spikes++;
    }

    // Gera corrente para o PRÓXIMO passo
    for (int i = 0; i < net->size; i++)
    {
        if (!net->spikes[i])
            continue;

        if (net->connections[i].list == NULL)
            continue;

        for (int j = 0; j < net->connections[i].count; j++)
        {
            Connection *c = &net->connections[i].list[j];

            if (c->target < 0 || c->target >= net->size)
                continue;

            if (c->delay < 1 || c->delay > net->max_synaptic_delay)
                continue;

            int delivery_slot =
                (net->delay_cursor + c->delay) %
                net->max_synaptic_delay;

            if (homeostasis_enabled &&
                net->neurons[i].type == NEURON_INHIBITORY)
            {
                net->pending_current[delivery_slot * net->size + c->target] +=
                    c->weight * net->homeostasis->inhibitory_gain;
            }
            else
            {
                net->pending_current[delivery_slot * net->size + c->target] +=
                    c->weight;
            }
        }
    }

    /* O spike atual ja foi transmitido com o peso anterior ao STDP. */
    if (!plasticity_apply_step(
            net->plasticity,
            net->neurons,
            net->connections,
            net->spikes,
            neuron_model_dt(&net->model_config)))
    {
        return -1;
    }

    if (reward_enabled &&
        (!reward_state_accumulate_candidates(
             net->reward,
             net->plasticity,
             (unsigned long long)net->step,
             neuron_model_dt(&net->model_config)) ||
         !reward_state_apply_pending(
             net->reward,
             net->neurons,
             net->connections,
             net->plasticity,
             (unsigned long long)net->step,
             neuron_model_dt(&net->model_config))))
    {
        return -1;
    }

    if (homeostasis_enabled &&
        !homeostasis_apply_step(
            net->homeostasis,
            neuron_model_base_threshold(&net->model_config),
            neuron_model_dt(&net->model_config),
            (unsigned long long)net->step + 1ULL,
            net->spikes,
            net->neurons,
            net->connections,
            net->plasticity))
    {
        return -1;
    }

    if (net->structural_plasticity != NULL &&
        !structural_plasticity_apply_step(
            net->structural_plasticity,
            net,
            (unsigned long long)net->step + 1ULL))
    {
        return -1;
    }

    next_slot =
        (net->delay_cursor + 1) %
        net->max_synaptic_delay;

    // Atualiza a corrente sináptica
    for (int i = 0; i < net->size; i++)
    {
        net->syn_current[i] =
            net->syn_current[i] * net->synaptic_decay +
            net->pending_current[next_slot * net->size + i];

        net->pending_current[next_slot * net->size + i] = 0.0;

        if (net->syn_current[i] > -1e-9 &&
            net->syn_current[i] <  1e-9)
        {
            net->syn_current[i] = 0.0;
        }
    }

    net->delay_cursor = next_slot;
    net->step++;

    return total_spikes;
}

static int network_connect_delayed_impl(
    Network *net,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection)
{
    int max_connections;

    if (net == NULL ||
        net->connections == NULL ||
        net->size <= 0 ||
        net->max_synaptic_delay <= 0)
    {
        return 0;
    }

    if (net->plasticity != NULL &&
        net->plasticity->config.enabled &&
        net->step > 0)
    {
        return 0;
    }


    if (net->homeostasis != NULL &&
        net->homeostasis->config.enabled &&
        net->step > 0)
    {
        return 0;
    }

    if (source < 0 || source >= net->size ||
        target < 0 || target >= net->size)
    {
        return 0;
    }

    if (!allow_self_connection && source == target)
        return 0;

    if (!isfinite(weight))
        return 0;

    if (delay < 1 || delay > net->max_synaptic_delay)
        return 0;

    ConnectionList *connections = &net->connections[source];
    max_connections = net->size;

    if (connections->count < 0 ||
        connections->count >= max_connections)
    {
        return 0;
    }

    if (connections->count > 0 && connections->list == NULL)
        return 0;

    for (int i = 0; i < connections->count; i++)
    {
        if (connections->list[i].target == target)
            return 0;
    }

    int new_count = connections->count + 1;
    Connection *new_list = realloc(
        connections->list,
        (size_t)new_count * sizeof(Connection));

    if (new_list == NULL)
        return 0;

    new_list[connections->count].target = target;
    new_list[connections->count].weight = weight;
    new_list[connections->count].delay = delay;

    connections->list = new_list;
    connections->count = new_count;

    plasticity_state_invalidate_index(net->plasticity);
    homeostasis_state_invalidate_targets(net->homeostasis);
    reward_state_invalidate(net->reward);

    return 1;
}

int network_connect(Network *net, int source, int target, double weight)
{
    return network_connect_ex(net, source, target, weight, 0);
}

int network_connect_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int allow_self_connection)
{
    return network_connect_delayed_ex(
        net,
        source,
        target,
        weight,
        1,
        allow_self_connection);
}

int network_connect_delayed(
    Network *net,
    int source,
    int target,
    double weight,
    int delay)
{
    return network_connect_delayed_ex(
        net,
        source,
        target,
        weight,
        delay,
        0);
}

int network_connect_delayed_ex(
    Network *net,
    int source,
    int target,
    double weight,
    int delay,
    int allow_self_connection)
{
    return network_connect_delayed_impl(
        net,
        source,
        target,
        weight,
        delay,
        allow_self_connection);
}

int network_set_neuron_type(
    Network *net,
    int neuron_id,
    NeuronType type)
{
    if (net == NULL || net->neurons == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (type != NEURON_EXCITATORY &&
        type != NEURON_INHIBITORY)
    {
        return 0;
    }

    if (net->plasticity != NULL &&
        net->plasticity->config.enabled &&
        net->step > 0)
    {
        return 0;
    }

    if (net->homeostasis != NULL &&
        net->homeostasis->config.enabled &&
        net->step > 0)
    {
        return 0;
    }

    net->neurons[neuron_id].type = type;
    plasticity_state_invalidate_index(net->plasticity);
    homeostasis_state_invalidate_targets(net->homeostasis);
    reward_state_invalidate(net->reward);
    return 1;
}

size_t network_connection_count(const Network *net)
{
    size_t count = 0;

    if (net == NULL || net->connections == NULL || net->size <= 0)
        return 0;

    for (int source = 0; source < net->size; source++)
    {
        if (net->connections[source].count < 0)
            return 0;

        count += (size_t)net->connections[source].count;
    }

    return count;
}

int network_get_connection(
    const Network *net,
    size_t connection_id,
    int *out_source,
    Connection **out_connection)
{
    size_t offset = 0;

    if (net == NULL || net->connections == NULL ||
        out_source == NULL || out_connection == NULL)
    {
        return 0;
    }

    for (int source = 0; source < net->size; source++)
    {
        const ConnectionList *list = &net->connections[source];
        size_t count;

        if (list->count < 0 || (list->count > 0 && list->list == NULL))
            return 0;

        count = (size_t)list->count;

        if (connection_id < offset + count)
        {
            *out_source = source;
            *out_connection = &list->list[connection_id - offset];
            return 1;
        }

        offset += count;
    }

    return 0;
}

int network_set_connection_weight(
    Network *net,
    size_t connection_id,
    double weight)
{
    int source;
    Connection *connection;

    if (!isfinite(weight) ||
        !network_get_connection(
            net,
            connection_id,
            &source,
            &connection))
    {
        return 0;
    }

    (void)source;
    connection->weight = weight;
    return 1;
}

int network_set_plasticity_config(
    Network *net,
    const MiniSNNPlasticityConfig *config)
{
    if (net == NULL || net->plasticity == NULL ||
        net->neurons == NULL || net->connections == NULL || config == NULL)
    {
        return 0;
    }

    if (net->reward != NULL && net->reward->config.enabled &&
        (!config->enabled ||
         config->learning_mode !=
             MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP))
    {
        return 0;
    }

    if (!plasticity_state_configure(
            net->plasticity,
            config,
            net->neurons,
            net->connections))
    {
        return 0;
    }

    if (net->reward != NULL && net->reward->config.enabled)
    {
        return reward_state_reset(
            net->reward,
            net->neurons,
            net->connections,
            net->plasticity);
    }

    return 1;
}

int network_set_reward_config(
    Network *net,
    const MiniSNNRewardConfig *config)
{
    if (net == NULL || net->reward == NULL || net->plasticity == NULL ||
        net->neurons == NULL || net->connections == NULL ||
        !reward_config_is_valid(config))
    {
        return 0;
    }

    if (config->enabled &&
        (!net->plasticity->config.enabled ||
         net->plasticity->config.learning_mode !=
             MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP))
    {
        return 0;
    }

    return reward_state_configure(
        net->reward,
        config,
        net->neurons,
        net->connections,
        net->plasticity);
}

int network_reset_reward_learning(Network *net)
{
    if (net == NULL || net->reward == NULL || net->plasticity == NULL ||
        net->neurons == NULL || net->connections == NULL)
    {
        return 0;
    }

    return reward_state_reset(
        net->reward,
        net->neurons,
        net->connections,
        net->plasticity);
}

int network_set_structural_plasticity_config(
    Network *net,
    const MiniSNNStructuralPlasticityConfig *config)
{
    StructuralPlasticityState *created = NULL;

    if (net == NULL || config == NULL || net->neurons == NULL ||
        net->connections == NULL || net->size <= 0 ||
        !structural_plasticity_config_is_valid(
            config, net->size, net->max_synaptic_delay,
            network_connection_count(net)))
        return 0;

    if (!config->enabled)
    {
        structural_plasticity_state_destroy(net->structural_plasticity);
        net->structural_plasticity = NULL;
        return 1;
    }

    if (!structural_plasticity_state_create(&created, net, config))
        return 0;
    structural_plasticity_state_destroy(net->structural_plasticity);
    net->structural_plasticity = created;
    return 1;
}

int network_reset_structural_plasticity(
    Network *net,
    MiniSNNStructuralResetMode mode)
{
    if (net == NULL || net->structural_plasticity == NULL)
        return 0;
    return structural_plasticity_reset(
        net->structural_plasticity, net, mode);
}

int network_set_homeostasis_config(
    Network *net,
    const MiniSNNHomeostasisConfig *config)
{
    MiniSNNNeuronModelCapabilities capabilities;

    if (net == NULL || net->homeostasis == NULL ||
        net->plasticity == NULL || net->neurons == NULL ||
        net->connections == NULL)
    {
        return 0;
    }

    capabilities = neuron_model_capabilities(net->model_config.model);
    if (!homeostasis_validate_capabilities(config, &capabilities))
        return 0;

    return homeostasis_state_configure(
        net->homeostasis,
        config,
        neuron_model_base_threshold(&net->model_config),
        net->neurons,
        net->connections,
        net->plasticity);
}

int network_reset_homeostasis(Network *net)
{
    if (net == NULL || net->homeostasis == NULL ||
        net->plasticity == NULL || net->neurons == NULL ||
        net->connections == NULL)
    {
        return 0;
    }

    return homeostasis_state_reset(
        net->homeostasis,
        neuron_model_base_threshold(&net->model_config),
        net->neurons,
        net->connections,
        net->plasticity);
}

void network_clear_connections(Network *net)
{
    if (net == NULL || net->connections == NULL)
        return;

    for (int i = 0; i < net->size; i++)
    {
        free(net->connections[i].list);
        net->connections[i].list = NULL;
        net->connections[i].count = 0;
    }

    plasticity_state_invalidate_index(net->plasticity);
    homeostasis_state_invalidate_targets(net->homeostasis);
    reward_state_invalidate(net->reward);
}

int network_set_external_current(Network *net, int neuron_id, double current)
{
    if (net == NULL || net->ext_current == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (!isfinite(current))
        return 0;

    net->ext_current[neuron_id] = current;
    return 1;
}

int network_add_external_current(Network *net, int neuron_id, double current)
{
    if (net == NULL || net->ext_current == NULL)
        return 0;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (!isfinite(current))
        return 0;

    double updated_current = net->ext_current[neuron_id] + current;

    if (!isfinite(updated_current))
        return 0;

    net->ext_current[neuron_id] = updated_current;
    return 1;
}

void network_clear_external_currents(Network *net)
{
    if (net == NULL || net->ext_current == NULL)
        return;

    for (int i = 0; i < net->size; i++)
        net->ext_current[i] = 0.0;
}

void network_destroy(Network *net)
{
    if (net == NULL)
        return;

    if (net->connections != NULL)
    {
        network_clear_connections(net);
        free(net->connections);
    }

    free(net->neurons);
    free(net->step_snapshot);
    free(net->spikes);
    free(net->syn_current);
    free(net->used_syn_current);
    free(net->pending_current);
    free(net->ext_current);

    if (net->plasticity != NULL)
    {
        plasticity_state_destroy(net->plasticity);
        free(net->plasticity);
    }

    if (net->homeostasis != NULL)
    {
        homeostasis_state_destroy(net->homeostasis);
        free(net->homeostasis);
    }

    if (net->reward != NULL)
    {
        reward_state_destroy(net->reward);
        free(net->reward);
    }

    structural_plasticity_state_destroy(net->structural_plasticity);
    net->structural_plasticity = NULL;

    network_reset_fields(net);
}

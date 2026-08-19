#include "minisnn.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Keep the historical scenario-runner stream exactly: topology generation is
 * public now, but existing scenario seeds must retain their old networks. */
static uint32_t factory_next(uint32_t *state)
{
    uint32_t value = *state == 0U ? UINT32_C(1) : *state;

    value = value * UINT32_C(1664525) + UINT32_C(1013904223);
    *state = value;
    return value;
}

static double factory_unit(uint32_t *state)
{
    return (double)(factory_next(state) >> 8U) / 16777216.0;
}

static int factory_valid(const MiniSNN *snn,
                         const MiniSNNTopologyFactoryConfig *config)
{
    int count;
    uint32_t index;

    if (snn == NULL || config == NULL ||
        config->kind > MINISNN_TOPOLOGY_FACTORY_FULLY_CONNECTED ||
        !isfinite(config->connection_probability) ||
        config->connection_probability < 0.0 ||
        config->connection_probability > 1.0 ||
        !isfinite(config->small_world_rewire_probability) ||
        config->small_world_rewire_probability < 0.0 ||
        config->small_world_rewire_probability > 1.0 ||
        config->delay == 0U || !isfinite(config->excitatory_weight) ||
        config->excitatory_weight <= 0.0 ||
        !isfinite(config->inhibitory_weight) || config->inhibitory_weight >= 0.0 ||
        (config->allow_self_connections != 0 &&
         config->allow_self_connections != 1) ||
        (config->allow_inhibitory_to_inhibitory != 0 &&
         config->allow_inhibitory_to_inhibitory != 1))
    {
        return 0;
    }
    count = minisnn_neuron_count(snn);
    if (count <= 0 || config->inhibitory_count > (uint32_t)count ||
        (config->kind == MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD &&
         (config->small_world_neighbors > (uint32_t)(count - 1) ||
          (config->small_world_neighbors % 2U) != 0U)) ||
        (config->required_connection_count > 0U &&
         config->required_connections == NULL))
    {
        return 0;
    }
    for (index = 0U; index < config->required_connection_count; ++index)
    {
        const MiniSNNTopologyFactoryRequiredConnection *required =
            &config->required_connections[index];
        if (required->source >= (uint32_t)count ||
            required->target >= (uint32_t)count || required->delay == 0U ||
            !isfinite(required->weight))
        {
            return 0;
        }
    }
    return 1;
}

static int factory_allowed(const MiniSNNTopologyFactoryConfig *config,
                           uint32_t neuron_count, uint32_t source,
                           uint32_t target)
{
    int source_inhibitory = source >= neuron_count - config->inhibitory_count;
    int target_inhibitory = target >= neuron_count - config->inhibitory_count;

    if (!config->allow_self_connections && source == target)
        return 0;
    return config->allow_inhibitory_to_inhibitory ||
           !source_inhibitory || !target_inhibitory;
}

static int factory_connect_with_weight(
    MiniSNN *snn, const MiniSNNTopologyFactoryConfig *config,
    uint32_t neuron_count, uint32_t source, uint32_t target,
    double weight, unsigned char *seen)
{
    size_t key = (size_t)source * neuron_count + target;

    if (!factory_allowed(config, neuron_count, source, target) ||
        seen[key] != 0U)
    {
        return 1;
    }
    if (!minisnn_connect_delayed_ex(snn, (int)source, (int)target, weight,
                                    (int)config->delay,
                                    config->allow_self_connections))
    {
        return 0;
    }
    seen[key] = 1U;
    return 1;
}

static int factory_connect(MiniSNN *snn,
                           const MiniSNNTopologyFactoryConfig *config,
                           uint32_t neuron_count, uint32_t source,
                           uint32_t target, unsigned char *seen)
{
    int inhibitory = source >= neuron_count - config->inhibitory_count;
    double weight = inhibitory ? config->inhibitory_weight :
                                 config->excitatory_weight;

    return factory_connect_with_weight(
        snn, config, neuron_count, source, target, weight, seen);
}

static int factory_choose_rewired_target(
    const MiniSNNTopologyFactoryConfig *config, uint32_t count,
    uint32_t source, const unsigned char *seen, uint32_t *state,
    uint32_t *out_target)
{
    uint32_t attempt;
    uint32_t target;

    for (attempt = 0U; attempt < count * 4U; ++attempt)
    {
        target = (uint32_t)(factory_unit(state) * (double)count);
        if (target >= count)
            target = count - 1U;
        if (seen[(size_t)source * count + target] == 0U &&
            factory_allowed(config, count, source, target))
        {
            *out_target = target;
            return 1;
        }
    }
    for (target = 0U; target < count; ++target)
    {
        if (seen[(size_t)source * count + target] == 0U &&
            factory_allowed(config, count, source, target))
        {
            *out_target = target;
            return 1;
        }
    }
    return 0;
}

MiniSNNTopologyFactoryConfig minisnn_topology_factory_default(void)
{
    MiniSNNTopologyFactoryConfig config;

    config.kind = MINISNN_TOPOLOGY_FACTORY_RANDOM;
    config.seed = UINT64_C(1);
    config.inhibitory_count = 0U;
    config.connection_probability = 0.20;
    config.small_world_neighbors = 4U;
    config.small_world_rewire_probability = 0.10;
    config.excitatory_weight = 100.0;
    config.inhibitory_weight = -75.0;
    config.delay = 1U;
    config.allow_self_connections = 0;
    config.allow_inhibitory_to_inhibitory = 1;
    config.required_connections = NULL;
    config.required_connection_count = 0U;
    return config;
}

int minisnn_topology_factory_build(
    MiniSNN *snn, const MiniSNNTopologyFactoryConfig *config)
{
    uint32_t state;
    uint32_t count;
    uint32_t source;
    unsigned char *seen;
    int ok = 0;

    if (!factory_valid(snn, config))
        return 0;
    count = (uint32_t)minisnn_neuron_count(snn);
    if ((size_t)count > SIZE_MAX / (size_t)count)
        return 0;
    seen = calloc((size_t)count * count, sizeof(*seen));
    if (seen == NULL)
        return 0;
    for (source = 0U; source < count; ++source)
    {
        MiniSNNNeuronType type = source >= count - config->inhibitory_count ?
            MINISNN_NEURON_INHIBITORY : MINISNN_NEURON_EXCITATORY;
        if (!minisnn_set_neuron_type(snn, (int)source, type))
            goto done;
    }
    for (source = 0U; source < config->required_connection_count; ++source)
    {
        const MiniSNNTopologyFactoryRequiredConnection *required =
            &config->required_connections[source];
        if (!factory_connect_with_weight(
                snn, config, count, required->source, required->target,
                required->weight, seen))
        {
            goto done;
        }
    }
    state = (uint32_t)config->seed;
    if (state == 0U)
        state = UINT32_C(1);
    if (config->kind == MINISNN_TOPOLOGY_FACTORY_FULLY_CONNECTED)
    {
        uint32_t target;
        for (source = 0U; source < count; ++source)
            for (target = 0U; target < count; ++target)
                if (!factory_connect(snn, config, count, source, target, seen))
                    goto done;
    }
    else if (config->kind == MINISNN_TOPOLOGY_FACTORY_RANDOM)
    {
        uint32_t target;
        for (source = 0U; source < count; ++source)
        {
            for (target = 0U; target < count; ++target)
            {
                if (!factory_allowed(config, count, source, target))
                    continue;
                if (factory_unit(&state) < config->connection_probability &&
                    !factory_connect(snn, config, count, source, target, seen))
                {
                    goto done;
                }
            }
        }
    }
    else
    {
        uint32_t half = config->small_world_neighbors / 2U;
        for (source = 0U; source < count; ++source)
        {
            uint32_t offset;
            for (offset = 1U; offset <= half; ++offset)
            {
                uint32_t side;
                for (side = 0U; side < 2U; ++side)
                {
                    uint32_t target = side == 0U ? (source + offset) % count :
                        (source + count - offset) % count;
                    if (factory_unit(&state) <
                        config->small_world_rewire_probability &&
                        !factory_choose_rewired_target(
                            config, count, source, seen, &state, &target))
                    {
                        continue;
                    }
                    if (seen[(size_t)source * count + target] != 0U ||
                        !factory_allowed(config, count, source, target))
                    {
                        continue;
                    }
                    if (!factory_connect(snn, config, count, source, target, seen))
                        goto done;
                }
            }
        }
    }
    ok = 1;
done:
    free(seen);
    return ok;
}
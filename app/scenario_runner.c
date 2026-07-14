#include "scenario_runner.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "minisnn.h"

#define COMMAND_BUFFER_SIZE 640
#define SCENARIO_MAX_NEURONS 1000
#define SCENARIO_HISTORY_PATH "results/scenarios/index.csv"
#define SCENARIO_HISTORY_HEADER \
    "timestamp,run_name,actual_run_name,run_path,config_path,topology,num_neurons,steps,dt,seed,recorded_neuron,total_connections,total_spikes,first_active_step,last_active_step,status\n"
#define MINISNN_VERSION "0.2"
#define TOPOLOGY_HASH_OFFSET 1469598103934665603ULL
#define TOPOLOGY_HASH_PRIME 1099511628211ULL

typedef struct
{
    int count;
    int self_count;
    int excitatory_count;
    int inhibitory_count;
    int excitatory_to_excitatory_count;
    int excitatory_to_inhibitory_count;
    int inhibitory_to_excitatory_count;
    int inhibitory_to_inhibitory_count;
    unsigned long long topology_signature;
    int indegree[SCENARIO_MAX_NEURONS];
    int outdegree[SCENARIO_MAX_NEURONS];
    double weight_sum;
    double weight_square_sum;
    double weight_min;
    double weight_max;
    double delay_sum;
    double delay_square_sum;
    int delay_min;
    int delay_max;
} ConnectivityStats;

typedef struct
{
    size_t count;
    double sum;
    double square_sum;
    double minimum;
    double maximum;
} WeightAggregate;

typedef struct
{
    int enabled;
    int snapshots_sampled;
    size_t total_connections;
    size_t sample_count;
    size_t *sample_ids;
    double *initial_sample_weights;
    FILE *history_file;
    int last_history_step;
    WeightAggregate initial_weights;
    WeightAggregate final_weights;
    MiniSNNPlasticityStats stats;
} PlasticityRunData;

typedef struct
{
    int enabled;
    FILE *history_file;
    FILE *threshold_file;
    int last_history_step;
    size_t sample_count;
    int sample_ids[SCENARIO_MAX_NEURONS];
    MiniSNNHomeostasisStats stats;
    double final_threshold_mean;
} HomeostasisRunData;

typedef struct
{
    int enabled;
    FILE *events_file;
    FILE *history_file;
    FILE *eligibility_file;
    int last_history_step;
    size_t total_connections;
    size_t sample_count;
    size_t *sample_ids;
    double *initial_weights;
    MiniSNNRewardStats stats;
    int scheduled_step_count;
    int first_event_step;
    int last_event_step;
} RewardRunData;

static void topology_hash_value(
    unsigned long long *hash,
    unsigned long long value)
{
    for (int byte_index = 0; byte_index < 8; byte_index++)
    {
        unsigned char byte = (unsigned char)((value >> (byte_index * 8)) & 0xffULL);
        *hash ^= (unsigned long long)byte;
        *hash *= TOPOLOGY_HASH_PRIME;
    }
}

static void topology_hash_connection(
    ConnectivityStats *stats,
    int source,
    int target,
    double weight,
    int delay,
    int source_is_inhibitory)
{
    unsigned long long weight_bits = 0ULL;

    memcpy(&weight_bits, &weight, sizeof(weight_bits));
    topology_hash_value(&stats->topology_signature, (unsigned long long)source);
    topology_hash_value(&stats->topology_signature, (unsigned long long)target);
    topology_hash_value(&stats->topology_signature, weight_bits);
    topology_hash_value(&stats->topology_signature, (unsigned long long)delay);
    topology_hash_value(
        &stats->topology_signature,
        (unsigned long long)source_is_inhibitory);
}

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message == NULL || error_message_size == 0)
        return;

    snprintf(error_message, error_message_size, "%s", message);
}

static int make_directory_if_needed(const char *path)
{
    char command[COMMAND_BUFFER_SIZE];

    if (snprintf(
            command,
            sizeof(command),
            "if not exist \"%s\" mkdir \"%s\"",
            path,
            path) >= (int)sizeof(command))
    {
        return 0;
    }

    return system(command) == 0;
}

static int directory_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int file_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void current_timestamp(char *out_timestamp, size_t out_timestamp_size)
{
    SYSTEMTIME now;

    GetLocalTime(&now);
    snprintf(
        out_timestamp,
        out_timestamp_size,
        "%04d%02d%02d_%02d%02d%02d",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond);
}

static int make_output_name(
    const char *run_name,
    int auto_unique_run,
    char *out_name,
    size_t out_name_size)
{
    char timestamp[32];

    if (snprintf(out_name, out_name_size, "%s", run_name) >= (int)out_name_size)
        return 0;

    if (!auto_unique_run)
        return 1;

    {
        char candidate_path[SCENARIO_OUTPUT_PATH_MAX];

        if (snprintf(
                candidate_path,
                sizeof(candidate_path),
                "results/scenarios/%s",
                out_name) >= (int)sizeof(candidate_path))
        {
            return 0;
        }

        if (!directory_exists(candidate_path))
            return 1;
    }

    current_timestamp(timestamp, sizeof(timestamp));

    for (int suffix = 0; suffix < 1000; suffix++)
    {
        char candidate[SCENARIO_ACTUAL_RUN_NAME_MAX];
        char candidate_path[SCENARIO_OUTPUT_PATH_MAX];

        if (suffix == 0)
        {
            if (snprintf(
                    candidate,
                    sizeof(candidate),
                    "%s_%s",
                    run_name,
                    timestamp) >= (int)sizeof(candidate))
            {
                return 0;
            }
        }
        else
        {
            if (snprintf(
                    candidate,
                    sizeof(candidate),
                    "%s_%s_%d",
                    run_name,
                    timestamp,
                    suffix + 1) >= (int)sizeof(candidate))
            {
                return 0;
            }
        }

        if (snprintf(
                candidate_path,
                sizeof(candidate_path),
                "results/scenarios/%s",
                candidate) >= (int)sizeof(candidate_path))
        {
            return 0;
        }

        if (!directory_exists(candidate_path))
        {
            snprintf(out_name, out_name_size, "%s", candidate);
            return 1;
        }
    }

    return 0;
}

static int ensure_output_directory(
    const ScenarioConfig *config,
    char *out_dir,
    char *actual_run_name,
    size_t actual_run_name_size)
{
    if (!make_directory_if_needed("results"))
        return 0;

    if (!make_directory_if_needed("results\\scenarios"))
        return 0;

    if (!make_output_name(
            config->run_name,
            config->auto_unique_run,
            actual_run_name,
            actual_run_name_size))
    {
        return 0;
    }

    if (snprintf(
            out_dir,
            SCENARIO_OUTPUT_PATH_MAX,
            "results/scenarios/%s",
            actual_run_name) >= SCENARIO_OUTPUT_PATH_MAX)
    {
        return 0;
    }

    return make_directory_if_needed(out_dir);
}

static int make_path(
    char *out_path,
    const char *directory,
    const char *filename)
{
    return snprintf(
               out_path,
               SCENARIO_OUTPUT_PATH_MAX,
               "%s/%s",
               directory,
               filename) < SCENARIO_OUTPUT_PATH_MAX;
}

static int copy_file_exact(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    FILE *output;
    unsigned char buffer[1024];
    size_t count;

    if (input == NULL)
        return 0;

    output = fopen(destination, "wb");

    if (output == NULL)
    {
        fclose(input);
        return 0;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0)
    {
        if (fwrite(buffer, 1, count, output) != count)
        {
            fclose(input);
            fclose(output);
            return 0;
        }
    }

    if (ferror(input))
    {
        fclose(input);
        fclose(output);
        return 0;
    }

    fclose(input);

    if (fclose(output) != 0)
        return 0;

    return 1;
}

static int write_config_used(
    const ScenarioConfig *config,
    const char *source_config_path,
    const char *destination,
    char *error_message,
    size_t error_message_size)
{
    if (source_config_path != NULL)
    {
        if (!copy_file_exact(source_config_path, destination))
        {
            set_error(error_message, error_message_size, "erro ao copiar config_used.ini");
            return 0;
        }

        return 1;
    }

    return scenario_config_save_file(
        destination,
        config,
        error_message,
        error_message_size);
}

static int calculate_inhibitory_count(const ScenarioConfig *config)
{
    int count = (int)((double)config->neurons *
                      config->inhibitory_fraction +
                      0.5);

    if (count < 0)
        return 0;

    if (count > config->neurons)
        return config->neurons;

    return count;
}

static int neuron_id_is_inhibitory(int neuron_id, int neurons, int inhibitory_count)
{
    return neuron_id >= neurons - inhibitory_count;
}

static const char *type_name(
    int neuron_id,
    int neurons,
    int inhibitory_count)
{
    return neuron_id_is_inhibitory(neuron_id, neurons, inhibitory_count) ?
        "INH" :
        "EXC";
}

static double outgoing_weight(
    const ScenarioConfig *config,
    int source,
    const int *neuron_is_inhibitory)
{
    if (neuron_is_inhibitory[source])
        return config->inhibitory_weight;

    return config->excitatory_weight;
}

static uint32_t rng_next(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static double rng_next_unit(uint32_t *state)
{
    return (double)(rng_next(state) >> 8) / 16777216.0;
}

static int set_neuron_types(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *neuron_is_inhibitory)
{
    for (int i = 0; i < config->neurons; i++)
    {
        neuron_is_inhibitory[i] = neuron_id_is_inhibitory(
            i,
            config->neurons,
            inhibitory_count);

        MiniSNNNeuronType type =
            neuron_is_inhibitory[i] ?
            MINISNN_NEURON_INHIBITORY :
            MINISNN_NEURON_EXCITATORY;

        if (!minisnn_set_neuron_type(snn, i, type))
            return 0;
    }

    return 1;
}

static int connection_is_allowed(
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    int source,
    int target)
{
    if (!config->allow_self_connections && source == target)
        return 0;

    if (!config->allow_inh_to_inh &&
        neuron_is_inhibitory[source] &&
        neuron_is_inhibitory[target])
    {
        return 0;
    }

    return 1;
}

static int connect_pair(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    int source,
    int target,
    ConnectivityStats *stats)
{
    double weight = outgoing_weight(config, source, neuron_is_inhibitory);

    if (!minisnn_connect_delayed_ex(
            snn,
            source,
            target,
            weight,
            config->delay,
            config->allow_self_connections))
    {
        return 0;
    }

    stats->count++;
    stats->indegree[target]++;
    stats->outdegree[source]++;
    stats->weight_sum += weight;
    stats->weight_square_sum += weight * weight;
    stats->delay_sum += (double)config->delay;
    stats->delay_square_sum += (double)config->delay * (double)config->delay;

    if (stats->count == 1 || weight < stats->weight_min)
        stats->weight_min = weight;
    if (stats->count == 1 || weight > stats->weight_max)
        stats->weight_max = weight;
    if (stats->count == 1 || config->delay < stats->delay_min)
        stats->delay_min = config->delay;
    if (stats->count == 1 || config->delay > stats->delay_max)
        stats->delay_max = config->delay;

    if (source == target)
        stats->self_count++;
    if (neuron_is_inhibitory[source])
        stats->inhibitory_count++;
    else
        stats->excitatory_count++;

    if (neuron_is_inhibitory[source])
    {
        if (neuron_is_inhibitory[target])
            stats->inhibitory_to_inhibitory_count++;
        else
            stats->inhibitory_to_excitatory_count++;
    }
    else if (neuron_is_inhibitory[target])
    {
        stats->excitatory_to_inhibitory_count++;
    }
    else
    {
        stats->excitatory_to_excitatory_count++;
    }

    topology_hash_connection(
        stats,
        source,
        target,
        weight,
        config->delay,
        neuron_is_inhibitory[source]);
    return 1;
}

static int build_chain(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    for (int source = 0; source < config->neurons - 1; source++)
    {
        if (!connection_is_allowed(
                config,
                neuron_is_inhibitory,
                source,
                source + 1))
        {
            continue;
        }

        if (!connect_pair(
                snn,
                config,
                neuron_is_inhibitory,
                source,
                source + 1,
                stats))
        {
            return 0;
        }
    }

    return 1;
}

static int build_ring(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    if (config->neurons == 1)
        return 1;

    if (!build_chain(snn, config, neuron_is_inhibitory, stats))
        return 0;

    if (!connection_is_allowed(
            config,
            neuron_is_inhibitory,
            config->neurons - 1,
            0))
    {
        return 1;
    }

    return connect_pair(
        snn,
        config,
        neuron_is_inhibitory,
        config->neurons - 1,
        0,
        stats);
}

static int build_all_to_all(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    for (int source = 0; source < config->neurons; source++)
    {
        for (int target = 0; target < config->neurons; target++)
        {
            if (!connection_is_allowed(
                    config,
                    neuron_is_inhibitory,
                    source,
                    target))
            {
                continue;
            }

            if (!connect_pair(
                    snn,
                    config,
                    neuron_is_inhibitory,
                    source,
                    target,
                stats))
            {
                return 0;
            }
        }
    }

    return 1;
}

static int build_random_like(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    uint32_t state = config->seed;

    if (state == 0U)
        state = 1U;

    for (int source = 0; source < config->neurons; source++)
    {
        for (int target = 0; target < config->neurons; target++)
        {
            if (!connection_is_allowed(
                    config,
                    neuron_is_inhibitory,
                    source,
                    target))
            {
                continue;
            }

            if (rng_next_unit(&state) < config->connection_probability)
            {
                if (!connect_pair(
                        snn,
                        config,
                        neuron_is_inhibitory,
                        source,
                        target,
                        stats))
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}

static int choose_rewired_target(
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    const int *used_targets,
    int source,
    uint32_t *state,
    int *out_target)
{
    int attempts = config->neurons * 4;

    for (int i = 0; i < attempts; i++)
    {
        int target = (int)(rng_next_unit(state) * (double)config->neurons);

        if (target >= config->neurons)
            target = config->neurons - 1;

        if (!used_targets[target] &&
            connection_is_allowed(
                config,
                neuron_is_inhibitory,
                source,
                target))
        {
            *out_target = target;
            return 1;
        }
    }

    for (int target = 0; target < config->neurons; target++)
    {
        if (!used_targets[target] &&
            connection_is_allowed(
                config,
                neuron_is_inhibitory,
                source,
                target))
        {
            *out_target = target;
            return 1;
        }
    }

    return 0;
}

static int build_small_world(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    uint32_t state = config->seed;
    int half_neighbors = config->small_world_neighbors / 2;
    int used_targets[SCENARIO_MAX_NEURONS];

    if (state == 0U)
        state = 1U;

    for (int source = 0; source < config->neurons; source++)
    {
        for (int i = 0; i < config->neurons; i++)
            used_targets[i] = 0;

        for (int offset = 1; offset <= half_neighbors; offset++)
        {
            int local_targets[2];

            local_targets[0] = (source + offset) % config->neurons;
            local_targets[1] =
                (source - offset + config->neurons) % config->neurons;

            for (int side = 0; side < 2; side++)
            {
                int target = local_targets[side];

                if (rng_next_unit(&state) <
                    config->small_world_rewire_probability)
                {
                    if (!choose_rewired_target(
                            config,
                            neuron_is_inhibitory,
                            used_targets,
                            source,
                            &state,
                            &target))
                    {
                        continue;
                    }
                }

                if (used_targets[target] ||
                    !connection_is_allowed(
                        config,
                        neuron_is_inhibitory,
                        source,
                        target))
                {
                    continue;
                }

                if (!connect_pair(
                        snn,
                        config,
                        neuron_is_inhibitory,
                        source,
                        target,
                        stats))
                {
                    return 0;
                }

                used_targets[target] = 1;
            }
        }
    }

    return 1;
}

static int layer_start(
    const ScenarioConfig *config,
    int layer)
{
    int base_size = config->neurons / config->feedforward_layers;
    int remainder = config->neurons % config->feedforward_layers;

    return layer * base_size + (layer < remainder ? layer : remainder);
}

static int layer_size(
    const ScenarioConfig *config,
    int layer)
{
    int base_size = config->neurons / config->feedforward_layers;
    int remainder = config->neurons % config->feedforward_layers;

    return base_size + (layer < remainder ? 1 : 0);
}

static int build_feedforward(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    uint32_t state = config->seed;

    if (state == 0U)
        state = 1U;

    for (int layer = 0; layer < config->feedforward_layers - 1; layer++)
    {
        int source_start = layer_start(config, layer);
        int source_size = layer_size(config, layer);
        int target_start = layer_start(config, layer + 1);
        int target_size = layer_size(config, layer + 1);

        for (int i = 0; i < source_size; i++)
        {
            int source = source_start + i;

            for (int j = 0; j < target_size; j++)
            {
                int target = target_start + j;

                if (!connection_is_allowed(
                        config,
                        neuron_is_inhibitory,
                        source,
                        target))
                {
                    continue;
                }

                if (rng_next_unit(&state) < config->connection_probability)
                {
                    if (!connect_pair(
                            snn,
                            config,
                            neuron_is_inhibitory,
                            source,
                            target,
                            stats))
                    {
                        return 0;
                    }
                }
            }
        }
    }

    return 1;
}

static int build_topology(
    MiniSNN *snn,
    const ScenarioConfig *config,
    const int *neuron_is_inhibitory,
    ConnectivityStats *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->topology_signature = TOPOLOGY_HASH_OFFSET;

    if (strcmp(config->topology, "chain") == 0)
        return build_chain(snn, config, neuron_is_inhibitory, stats);

    if (strcmp(config->topology, "ring") == 0)
        return build_ring(snn, config, neuron_is_inhibitory, stats);

    if (strcmp(config->topology, "all_to_all") == 0)
        return build_all_to_all(
            snn,
            config,
            neuron_is_inhibitory,
            stats);

    if (strcmp(config->topology, "random") == 0)
        return build_random_like(
            snn,
            config,
            neuron_is_inhibitory,
            stats);

    if (strcmp(config->topology, "random_balanced") == 0)
        return build_random_like(
            snn,
            config,
            neuron_is_inhibitory,
            stats);

    if (strcmp(config->topology, "small_world") == 0)
        return build_small_world(
            snn,
            config,
            neuron_is_inhibitory,
            stats);

    if (strcmp(config->topology, "feedforward") == 0)
        return build_feedforward(
            snn,
            config,
            neuron_is_inhibitory,
            stats);

    return 0;
}

static const char *public_type_name(MiniSNNNeuronType type)
{
    return type == MINISNN_NEURON_INHIBITORY ? "INH" : "EXC";
}

static void weight_aggregate_add(WeightAggregate *aggregate, double weight)
{
    if (aggregate->count == 0)
    {
        aggregate->minimum = weight;
        aggregate->maximum = weight;
    }
    else
    {
        if (weight < aggregate->minimum)
            aggregate->minimum = weight;
        if (weight > aggregate->maximum)
            aggregate->maximum = weight;
    }

    aggregate->count++;
    aggregate->sum += weight;
    aggregate->square_sum += weight * weight;
}

static int collect_weight_aggregate(
    const MiniSNN *snn,
    WeightAggregate *aggregate)
{
    size_t connection_count;

    if (snn == NULL || aggregate == NULL)
        return 0;

    memset(aggregate, 0, sizeof(*aggregate));
    connection_count = minisnn_connection_count(snn);

    for (size_t connection_id = 0;
         connection_id < connection_count;
         connection_id++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, connection_id, &connection))
            return 0;

        if (connection.plasticity_eligible)
            weight_aggregate_add(aggregate, connection.weight);
    }

    return 1;
}

static double weight_aggregate_mean(const WeightAggregate *aggregate)
{
    return aggregate->count > 0 ?
        aggregate->sum / (double)aggregate->count :
        0.0;
}

static double weight_aggregate_std(const WeightAggregate *aggregate)
{
    double mean;
    double variance;

    if (aggregate->count == 0)
        return 0.0;

    mean = weight_aggregate_mean(aggregate);
    variance = aggregate->square_sum / (double)aggregate->count - mean * mean;
    return variance > 0.0 ? sqrt(variance) : 0.0;
}

static size_t distributed_sample_id(
    size_t total,
    size_t sample_count,
    size_t sample_index)
{
    if (sample_count <= 1U)
        return 0U;

    return (sample_index * (total - 1U)) / (sample_count - 1U);
}

static void plasticity_run_data_close(PlasticityRunData *data)
{
    if (data == NULL)
        return;

    if (data->history_file != NULL)
        fclose(data->history_file);

    free(data->sample_ids);
    free(data->initial_sample_weights);
    memset(data, 0, sizeof(*data));
}

static int write_initial_weights(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const char *output_dir,
    const PlasticityRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;

    if (!config->plasticity_record_weights)
        return 1;

    if (!make_path(path, output_dir, "weights_initial.csv"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "connection_id,source,target,source_type,target_type,delay,weight,eligible,sampled\n") < 0)
    {
        fclose(file);
        return 0;
    }

    for (size_t i = 0; i < data->sample_count; i++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, data->sample_ids[i], &connection) ||
            fprintf(
                file,
                "%llu,%llu,%llu,%s,%s,%u,%.17g,%d,%d\n",
                (unsigned long long)data->sample_ids[i],
                (unsigned long long)connection.source,
                (unsigned long long)connection.target,
                public_type_name(connection.source_type),
                public_type_name(connection.target_type),
                connection.delay,
                connection.weight,
                connection.plasticity_eligible,
                data->snapshots_sampled) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int write_weight_history_step(
    const MiniSNN *snn,
    PlasticityRunData *data,
    int step)
{
    if (data->history_file == NULL)
        return 1;

    for (size_t i = 0; i < data->sample_count; i++)
    {
        MiniSNNConnectionInfo connection;

        if (!minisnn_get_connection(snn, data->sample_ids[i], &connection) ||
            fprintf(
                data->history_file,
                "%d,%llu,%llu,%llu,%.17g\n",
                step,
                (unsigned long long)data->sample_ids[i],
                (unsigned long long)connection.source,
                (unsigned long long)connection.target,
                connection.weight) < 0)
        {
            return 0;
        }
    }

    data->last_history_step = step;
    return 1;
}

static int plasticity_run_data_prepare(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const char *output_dir,
    PlasticityRunData *data)
{
    size_t limit;
    int needs_sample;

    memset(data, 0, sizeof(*data));
    data->enabled = config->plasticity_enabled;
    data->last_history_step = -1;

    if (!data->enabled)
        return 1;

    data->total_connections = minisnn_connection_count(snn);

    if (!collect_weight_aggregate(snn, &data->initial_weights))
        return 0;

    needs_sample = config->plasticity_record_weights ||
        config->plasticity_record_history;
    limit = (size_t)config->plasticity_record_connection_limit;
    data->sample_count = needs_sample ? data->total_connections : 0U;

    if (data->sample_count > limit)
        data->sample_count = limit;

    data->snapshots_sampled =
        data->sample_count < data->total_connections;

    if (data->sample_count > 0)
    {
        data->sample_ids = malloc(
            data->sample_count * sizeof(*data->sample_ids));
        data->initial_sample_weights = malloc(
            data->sample_count * sizeof(*data->initial_sample_weights));

        if (data->sample_ids == NULL ||
            data->initial_sample_weights == NULL)
        {
            plasticity_run_data_close(data);
            return 0;
        }

        for (size_t i = 0; i < data->sample_count; i++)
        {
            size_t connection_id = data->snapshots_sampled ?
                distributed_sample_id(
                    data->total_connections,
                    data->sample_count,
                    i) :
                i;

            data->sample_ids[i] = connection_id;

            if (!minisnn_get_connection_weight(
                    snn,
                    connection_id,
                    &data->initial_sample_weights[i]))
            {
                plasticity_run_data_close(data);
                return 0;
            }
        }
    }

    if (!write_initial_weights(snn, config, output_dir, data))
    {
        plasticity_run_data_close(data);
        return 0;
    }

    if (config->plasticity_record_history)
    {
        char path[SCENARIO_OUTPUT_PATH_MAX];

        if (!make_path(path, output_dir, "weight_history.csv"))
        {
            plasticity_run_data_close(data);
            return 0;
        }

        data->history_file = fopen(path, "w");

        if (data->history_file == NULL ||
            fprintf(
                data->history_file,
                "step,connection_id,source,target,weight\n") < 0 ||
            !write_weight_history_step(snn, data, 0))
        {
            plasticity_run_data_close(data);
            return 0;
        }
    }

    return 1;
}

static int write_final_weights(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const char *output_dir,
    const PlasticityRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;

    if (!config->plasticity_record_weights)
        return 1;

    if (!make_path(path, output_dir, "weights_final.csv"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "connection_id,source,target,source_type,target_type,delay,weight,eligible,sampled,initial_weight,final_weight,signed_change,absolute_change\n") < 0)
    {
        fclose(file);
        return 0;
    }

    for (size_t i = 0; i < data->sample_count; i++)
    {
        MiniSNNConnectionInfo connection;
        double change;

        if (!minisnn_get_connection(snn, data->sample_ids[i], &connection))
        {
            fclose(file);
            return 0;
        }

        change = connection.weight - data->initial_sample_weights[i];

        if (fprintf(
                file,
                "%llu,%llu,%llu,%s,%s,%u,%.17g,%d,%d,%.17g,%.17g,%.17g,%.17g\n",
                (unsigned long long)data->sample_ids[i],
                (unsigned long long)connection.source,
                (unsigned long long)connection.target,
                public_type_name(connection.source_type),
                public_type_name(connection.target_type),
                connection.delay,
                connection.weight,
                connection.plasticity_eligible,
                data->snapshots_sampled,
                data->initial_sample_weights[i],
                connection.weight,
                change,
                fabs(change)) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int write_plasticity_metrics(
    const ScenarioConfig *config,
    const char *output_dir,
    const PlasticityRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    double modified_fraction = data->stats.eligible_connections > 0 ?
        (double)data->stats.modified_connections /
            (double)data->stats.eligible_connections :
        0.0;
    double mean_absolute_change = data->stats.eligible_connections > 0 ?
        data->stats.total_absolute_change /
            (double)data->stats.eligible_connections :
        0.0;
    double mean_signed_change = data->stats.eligible_connections > 0 ?
        data->stats.total_signed_change /
            (double)data->stats.eligible_connections :
        0.0;

    if (!make_path(path, output_dir, "plasticity_metrics.csv"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "plasticity_enabled,plasticity_rule,plasticity_eligible_connection_count,plasticity_modified_connection_count,plasticity_modified_connection_fraction,plasticity_potentiation_events,plasticity_depression_events,plasticity_clamp_min_events,plasticity_clamp_max_events,plasticity_initial_weight_mean,plasticity_initial_weight_min,plasticity_initial_weight_max,plasticity_initial_weight_std,plasticity_final_weight_mean,plasticity_final_weight_min,plasticity_final_weight_max,plasticity_final_weight_std,plasticity_total_signed_change,plasticity_total_absolute_change,plasticity_mean_absolute_change,plasticity_max_absolute_change,plasticity_mean_signed_change\n"
            "%s,%s,%llu,%llu,%.17g,%llu,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
            config->plasticity_enabled ? "true" : "false",
            config->plasticity_rule,
            (unsigned long long)data->stats.eligible_connections,
            (unsigned long long)data->stats.modified_connections,
            modified_fraction,
            data->stats.potentiation_events,
            data->stats.depression_events,
            data->stats.clamp_min_events,
            data->stats.clamp_max_events,
            weight_aggregate_mean(&data->initial_weights),
            data->initial_weights.count > 0 ? data->initial_weights.minimum : 0.0,
            data->initial_weights.count > 0 ? data->initial_weights.maximum : 0.0,
            weight_aggregate_std(&data->initial_weights),
            weight_aggregate_mean(&data->final_weights),
            data->final_weights.count > 0 ? data->final_weights.minimum : 0.0,
            data->final_weights.count > 0 ? data->final_weights.maximum : 0.0,
            weight_aggregate_std(&data->final_weights),
            data->stats.total_signed_change,
            data->stats.total_absolute_change,
            mean_absolute_change,
            data->stats.max_absolute_change,
            mean_signed_change) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int write_stdp_report(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const char *output_dir,
    const PlasticityRunData *data,
    double simulation_seconds)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;

    if (!make_path(path, output_dir, "stdp_report.txt"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "MINISNN - RELATORIO DE PLASTICIDADE STDP\n\n"
            "1. Identificacao da execucao\n"
            "run_name=%s\nactual_run_name=%s\ntopology=%s\nsteps=%d\n\n"
            "2. Configuracao STDP\n"
            "rule=%s\na_plus=%.17g\na_minus=%.17g\ntau_plus=%.17g\ntau_minus=%.17g\n"
            "trace_increment=%.17g\nweight_min=%.17g\nweight_max=%.17g\n\n"
            "3. Convencao temporal\n"
            "O spike atual e transmitido com o peso anterior. O novo peso vale apenas para transmissoes futuras.\n"
            "A referencia temporal pre-sinaptica e a emissao do spike, nao sua chegada apos o delay.\n\n"
            "4. Conexoes elegiveis\n"
            "total_connections=%llu\neligible_connections=%llu\n"
            "Somente sinapses com origem EXC e peso nao negativo sao elegiveis.\n\n"
            "5. Pesos iniciais\nmean=%.17g\nmin=%.17g\nmax=%.17g\nstd=%.17g\n\n"
            "6. Pesos finais\nmean=%.17g\nmin=%.17g\nmax=%.17g\nstd=%.17g\n\n"
            "7. Potencializacao e depressao\npotentiation_events=%llu\ndepression_events=%llu\n"
            "total_signed_change=%.17g\ntotal_absolute_change=%.17g\n\n"
            "8. Limites de peso\nclamp_min_events=%llu\nclamp_max_events=%llu\n\n"
            "9. Conexoes mais alteradas\nConsulte weights_final.csv para os deltas individuais registrados.\n\n"
            "10. Amostragem e registro\nrecorded_connections=%llu\ncomplete_snapshot=%s\n"
            "sampling_rule=%s\n\n"
            "11. Desempenho\nsimulation_time_seconds=%.6f\n\n"
            "12. Avisos e limitacoes\n"
            "Esta e uma regra STDP aditiva simplificada e experimental.\n"
            "Sinapses inibitorias permaneceram fixas nesta execucao.\n"
            "Aumento de peso nao prova aprendizado de uma tarefa.\n"
            "A implementacao nao representa toda a plasticidade biologica e nao inclui homeostase.\n",
            config->run_name,
            result->actual_run_name,
            config->topology,
            config->steps,
            config->plasticity_rule,
            config->plasticity_a_plus,
            config->plasticity_a_minus,
            config->plasticity_tau_plus,
            config->plasticity_tau_minus,
            config->plasticity_trace_increment,
            config->plasticity_weight_min,
            config->plasticity_weight_max,
            (unsigned long long)data->total_connections,
            (unsigned long long)data->stats.eligible_connections,
            weight_aggregate_mean(&data->initial_weights),
            data->initial_weights.count > 0 ? data->initial_weights.minimum : 0.0,
            data->initial_weights.count > 0 ? data->initial_weights.maximum : 0.0,
            weight_aggregate_std(&data->initial_weights),
            weight_aggregate_mean(&data->final_weights),
            data->final_weights.count > 0 ? data->final_weights.minimum : 0.0,
            data->final_weights.count > 0 ? data->final_weights.maximum : 0.0,
            weight_aggregate_std(&data->final_weights),
            data->stats.potentiation_events,
            data->stats.depression_events,
            data->stats.total_signed_change,
            data->stats.total_absolute_change,
            data->stats.clamp_min_events,
            data->stats.clamp_max_events,
            (unsigned long long)data->sample_count,
            data->snapshots_sampled ? "false" : "true",
            data->snapshots_sampled ?
                "floor(k*(E-1)/(L-1)); L=1 seleciona connection_id 0" :
                "todas as conexoes em ordem plana deterministica",
            simulation_seconds) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int plasticity_run_data_finalize(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    PlasticityRunData *data,
    double simulation_seconds)
{
    if (!data->enabled)
        return 1;

    if (data->history_file != NULL &&
        data->last_history_step != config->steps &&
        !write_weight_history_step(snn, data, config->steps))
    {
        return 0;
    }

    if (data->history_file != NULL)
    {
        FILE *history_file = data->history_file;
        data->history_file = NULL;

        if (fclose(history_file) != 0)
            return 0;
    }

    if (!collect_weight_aggregate(snn, &data->final_weights) ||
        !minisnn_get_plasticity_stats(snn, &data->stats) ||
        !write_final_weights(snn, config, result->output_directory, data) ||
        !write_plasticity_metrics(config, result->output_directory, data) ||
        !write_stdp_report(
            config,
            result,
            result->output_directory,
            data,
            simulation_seconds))
    {
        return 0;
    }

    return 1;
}

static void homeostasis_run_data_close(HomeostasisRunData *data)
{
    if (data == NULL)
        return;
    if (data->history_file != NULL)
        fclose(data->history_file);
    if (data->threshold_file != NULL)
        fclose(data->threshold_file);
    data->history_file = NULL;
    data->threshold_file = NULL;
}

static int collect_homeostasis_snapshot(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    double *population_rate,
    double *threshold_mean,
    double *threshold_min,
    double *threshold_max,
    double *incoming_mean,
    double *incoming_error_mean)
{
    double rate_sum = 0.0;
    double threshold_sum = 0.0;
    double incoming_sum = 0.0;
    double incoming_error_sum = 0.0;

    *threshold_min = 0.0;
    *threshold_max = 0.0;

    for (int i = 0; i < config->neurons; i++)
    {
        double rate;
        double threshold;
        double initial_incoming;
        double current_incoming;

        if (!minisnn_get_neuron_rate_trace(snn, i, &rate) ||
            !minisnn_get_neuron_effective_threshold(snn, i, &threshold) ||
            !minisnn_get_initial_incoming_exc_sum(snn, i, &initial_incoming) ||
            !minisnn_get_current_incoming_exc_sum(snn, i, &current_incoming))
        {
            return 0;
        }

        if (!isfinite(rate) || !isfinite(threshold) ||
            !isfinite(initial_incoming) || !isfinite(current_incoming))
        {
            return 0;
        }

        rate_sum += rate;
        threshold_sum += threshold;
        incoming_sum += current_incoming;
        incoming_error_sum += fabs(current_incoming - initial_incoming);

        if (i == 0 || threshold < *threshold_min)
            *threshold_min = threshold;
        if (i == 0 || threshold > *threshold_max)
            *threshold_max = threshold;
    }

    *population_rate = rate_sum / (double)config->neurons;
    *threshold_mean = threshold_sum / (double)config->neurons;
    *incoming_mean = incoming_sum / (double)config->neurons;
    *incoming_error_mean = incoming_error_sum / (double)config->neurons;
    return 1;
}

static int write_homeostasis_history_step(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    HomeostasisRunData *data,
    int step)
{
    double population_rate;
    double threshold_mean;
    double threshold_min;
    double threshold_max;
    double incoming_mean;
    double incoming_error_mean;
    double inhibitory_gain;
    MiniSNNHomeostasisStats stats;
    double scaling_factor_mean;

    if (data->history_file == NULL || data->threshold_file == NULL)
        return 1;

    if (!collect_homeostasis_snapshot(
            snn,
            config,
            &population_rate,
            &threshold_mean,
            &threshold_min,
            &threshold_max,
            &incoming_mean,
            &incoming_error_mean) ||
        !minisnn_get_inhibitory_gain(snn, &inhibitory_gain) ||
        !minisnn_get_homeostasis_stats(snn, &stats))
    {
        return 0;
    }

    scaling_factor_mean = stats.scaling_factor_count > 0ULL ?
        stats.scaling_factor_sum / (double)stats.scaling_factor_count : 1.0;

    if (fprintf(
            data->history_file,
            "%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu\n",
            step,
            population_rate,
            config->homeostasis_target_rate,
            population_rate - config->homeostasis_target_rate,
            threshold_mean,
            threshold_min,
            threshold_max,
            inhibitory_gain,
            incoming_mean,
            incoming_error_mean,
            scaling_factor_mean,
            stats.scaling_events) < 0)
    {
        return 0;
    }

    for (size_t i = 0; i < data->sample_count; i++)
    {
        double rate;
        double threshold;
        int neuron_id = data->sample_ids[i];

        if (!minisnn_get_neuron_rate_trace(snn, neuron_id, &rate) ||
            !minisnn_get_neuron_effective_threshold(snn, neuron_id, &threshold) ||
            fprintf(
                data->threshold_file,
                "%d,%d,%.17g,%.17g,%.17g,1\n",
                step,
                neuron_id,
                rate,
                threshold,
                config->v_threshold) < 0)
        {
            return 0;
        }
    }

    data->last_history_step = step;
    return 1;
}

static int homeostasis_run_data_prepare(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const char *output_dir,
    HomeostasisRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    size_t limit;

    memset(data, 0, sizeof(*data));
    data->enabled = config->homeostasis_enabled;
    data->last_history_step = -1;

    if (!data->enabled)
        return 1;

    limit = (size_t)config->homeostasis_record_neuron_limit;
    data->sample_count = (size_t)config->neurons < limit ?
        (size_t)config->neurons : limit;

    for (size_t i = 0; i < data->sample_count; i++)
    {
        data->sample_ids[i] = (int)distributed_sample_id(
            (size_t)config->neurons, data->sample_count, i);
    }

    if (!make_path(path, output_dir, "homeostasis_history.csv"))
        return 0;
    data->history_file = fopen(path, "w");
    if (data->history_file == NULL || fprintf(
            data->history_file,
            "step,population_rate,target_rate,rate_error,threshold_mean,threshold_min,threshold_max,inhibitory_gain,incoming_exc_sum_mean,incoming_exc_sum_error_mean,scaling_factor_mean,scaling_events_cumulative\n") < 0)
    {
        homeostasis_run_data_close(data);
        return 0;
    }

    if (!make_path(path, output_dir, "threshold_history.csv"))
    {
        homeostasis_run_data_close(data);
        return 0;
    }
    data->threshold_file = fopen(path, "w");
    if (data->threshold_file == NULL || fprintf(
            data->threshold_file,
            "step,neuron_id,rate_trace,effective_threshold,initial_threshold,sampled\n") < 0 ||
        !write_homeostasis_history_step(snn, config, data, 0))
    {
        homeostasis_run_data_close(data);
        return 0;
    }

    return 1;
}

static int write_homeostasis_neurons(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const HomeostasisRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    size_t count = data->sample_count > 0 ?
        data->sample_count : (size_t)config->neurons;

    if (!make_path(path, result->output_directory, "homeostasis_neurons.csv"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "neuron_id,neuron_type,rate_trace,target_rate,rate_error,initial_threshold,final_threshold,threshold_change,initial_incoming_exc_sum,final_incoming_exc_sum,incoming_exc_sum_change,sampled\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        int neuron_id = data->sample_count > 0 ? data->sample_ids[i] : (int)i;
        double rate;
        double threshold;
        double initial_sum;
        double final_sum;

        if (!minisnn_get_neuron_rate_trace(snn, neuron_id, &rate) ||
            !minisnn_get_neuron_effective_threshold(snn, neuron_id, &threshold) ||
            !minisnn_get_initial_incoming_exc_sum(snn, neuron_id, &initial_sum) ||
            !minisnn_get_current_incoming_exc_sum(snn, neuron_id, &final_sum) ||
            fprintf(
                file,
                "%d,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,1\n",
                neuron_id,
                neuron_id_is_inhibitory(
                    neuron_id, config->neurons, result->inhibitory_count) ?
                    "INH" : "EXC",
                rate,
                config->homeostasis_target_rate,
                rate - config->homeostasis_target_rate,
                config->v_threshold,
                threshold,
                threshold - config->v_threshold,
                initial_sum,
                final_sum,
                final_sum - initial_sum) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int write_homeostasis_metrics(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    HomeostasisRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    double population_rate;
    double threshold_mean;
    double threshold_min;
    double threshold_max;
    double incoming_mean;
    double incoming_error_mean;
    double initial_incoming_mean = 0.0;
    double threshold_abs_change = 0.0;
    double gain;
    double rate_mean;
    double rate_error_mean;
    double rate_error_abs_mean;
    double scaling_factor_mean;

    if (!collect_homeostasis_snapshot(
            snn, config, &population_rate, &threshold_mean, &threshold_min,
            &threshold_max, &incoming_mean, &incoming_error_mean) ||
        !minisnn_get_inhibitory_gain(snn, &gain) ||
        !minisnn_get_homeostasis_stats(snn, &data->stats))
    {
        return 0;
    }

    for (int i = 0; i < config->neurons; i++)
    {
        double initial_sum;
        double threshold;
        if (!minisnn_get_initial_incoming_exc_sum(snn, i, &initial_sum) ||
            !minisnn_get_neuron_effective_threshold(snn, i, &threshold))
        {
            return 0;
        }
        initial_incoming_mean += initial_sum;
        threshold_abs_change += fabs(threshold - config->v_threshold);
    }
    initial_incoming_mean /= (double)config->neurons;
    threshold_abs_change /= (double)config->neurons;

    rate_mean = data->stats.population_rate_sample_count > 0ULL ?
        data->stats.population_rate_sum /
            (double)data->stats.population_rate_sample_count : 0.0;
    rate_error_mean = data->stats.population_rate_sample_count > 0ULL ?
        data->stats.rate_error_sum /
            (double)data->stats.population_rate_sample_count : 0.0;
    rate_error_abs_mean = data->stats.population_rate_sample_count > 0ULL ?
        data->stats.rate_error_absolute_sum /
            (double)data->stats.population_rate_sample_count : 0.0;
    scaling_factor_mean = data->stats.scaling_factor_count > 0ULL ?
        data->stats.scaling_factor_sum /
            (double)data->stats.scaling_factor_count : 1.0;
    data->final_threshold_mean = threshold_mean;

    if (!make_path(path, result->output_directory, "homeostasis_metrics.csv"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "homeostasis_enabled,homeostasis_intrinsic_enabled,homeostasis_synaptic_scaling_enabled,homeostasis_inhibitory_gain_enabled,homeostasis_target_rate,homeostasis_update_interval_steps,homeostasis_update_count,homeostasis_population_rate_final,homeostasis_population_rate_mean,homeostasis_population_rate_min,homeostasis_population_rate_max,homeostasis_rate_error_final,homeostasis_rate_error_mean,homeostasis_rate_error_mean_absolute,homeostasis_threshold_initial_mean,homeostasis_threshold_final_mean,homeostasis_threshold_final_min,homeostasis_threshold_final_max,homeostasis_threshold_mean_absolute_change,homeostasis_threshold_modified_neuron_count,homeostasis_threshold_modified_neuron_fraction,homeostasis_threshold_increase_events,homeostasis_threshold_decrease_events,homeostasis_threshold_clamp_min_events,homeostasis_threshold_clamp_max_events,homeostasis_scaling_events,homeostasis_scaling_connections_modified,homeostasis_scaling_factor_mean,homeostasis_scaling_factor_min,homeostasis_scaling_factor_max,homeostasis_initial_incoming_exc_sum_mean,homeostasis_final_incoming_exc_sum_mean,homeostasis_incoming_exc_sum_error_mean_absolute,homeostasis_scaling_total_signed_change,homeostasis_scaling_total_absolute_change,homeostasis_scaling_zero_sum_skips,homeostasis_scaling_clamp_min_events,homeostasis_scaling_clamp_max_events,homeostasis_inhibitory_gain_initial,homeostasis_inhibitory_gain_final,homeostasis_inhibitory_gain_min_observed,homeostasis_inhibitory_gain_max_observed,homeostasis_inhibitory_gain_increase_events,homeostasis_inhibitory_gain_decrease_events,homeostasis_inhibitory_gain_clamp_min_events,homeostasis_inhibitory_gain_clamp_max_events,homeostasis_inhibitory_connection_count\n"
            "true,%s,%s,%s,%.17g,%d,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%.17g,%llu,%llu,%llu,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%llu,%llu,%d\n",
            config->homeostasis_enabled && config->homeostasis_intrinsic_enabled ?
                "true" : "false",
            config->homeostasis_enabled &&
                config->homeostasis_synaptic_scaling_enabled ? "true" : "false",
            config->homeostasis_enabled &&
                config->homeostasis_inhibitory_gain_enabled ? "true" : "false",
            config->homeostasis_target_rate,
            config->homeostasis_update_interval_steps,
            data->stats.update_count,
            population_rate,
            rate_mean,
            data->stats.population_rate_sample_count > 0ULL ?
                data->stats.population_rate_min : 0.0,
            data->stats.population_rate_sample_count > 0ULL ?
                data->stats.population_rate_max : 0.0,
            population_rate - config->homeostasis_target_rate,
            rate_error_mean,
            rate_error_abs_mean,
            config->v_threshold,
            threshold_mean,
            threshold_min,
            threshold_max,
            threshold_abs_change,
            data->stats.threshold_modified_neuron_count,
            (double)data->stats.threshold_modified_neuron_count /
                (double)config->neurons,
            data->stats.threshold_increase_events,
            data->stats.threshold_decrease_events,
            data->stats.threshold_clamp_min_events,
            data->stats.threshold_clamp_max_events,
            data->stats.scaling_events,
            data->stats.scaling_connections_modified,
            scaling_factor_mean,
            data->stats.scaling_factor_count > 0ULL ?
                data->stats.scaling_factor_min : 1.0,
            data->stats.scaling_factor_count > 0ULL ?
                data->stats.scaling_factor_max : 1.0,
            initial_incoming_mean,
            incoming_mean,
            incoming_error_mean,
            data->stats.total_scaling_signed_change,
            data->stats.total_scaling_absolute_change,
            data->stats.scaling_zero_sum_skips,
            data->stats.scaling_clamp_min_events,
            data->stats.scaling_clamp_max_events,
            config->homeostasis_inhibitory_gain_initial,
            gain,
            data->stats.inhibitory_gain_min_observed,
            data->stats.inhibitory_gain_max_observed,
            data->stats.inhibitory_gain_increase_events,
            data->stats.inhibitory_gain_decrease_events,
            data->stats.inhibitory_gain_clamp_min_events,
            data->stats.inhibitory_gain_clamp_max_events,
            result->inhibitory_to_excitatory_count +
                result->inhibitory_to_inhibitory_count) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int write_homeostasis_reports(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const HomeostasisRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    const char *text =
        "Estes mecanismos simplificados nao garantem estabilidade universal; "
        "silencio ou hiperatividade ainda podem ocorrer.";

    if (!make_path(path, result->output_directory, "homeostasis_report.txt"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "MINISNN - RELATORIO DE HOMEOSTASE\n\n"
            "1. Identificacao da execucao\nrun_name=%s\n\n"
            "2. Configuracao homeostatica\ntarget_rate=%.17g\ninterval=%d\n\n"
            "3. Ordem temporal\nLIF -> transmissao -> STDP -> rate trace -> homeostase.\n\n"
            "4. Taxa-alvo e taxa observada\nfinal_population_rate=%.17g\n\n"
            "5. Adaptacao de threshold\neventos=%llu\n\n"
            "6. Escalonamento sinaptico\neventos=%llu\n\n"
            "7. Ganho inibitorio\nfinal=%.17g\n\n"
            "8. Interacao com STDP\nScaling ocorre depois do STDP e possui estatisticas separadas.\n\n"
            "9. Estado inicial e final\nConsulte homeostasis_neurons.csv.\n\n"
            "10. Clamps e limites\nthreshold_min=%.17g threshold_max=%.17g\n\n"
            "11. Amostragem\nAmostragem deterministica distribuida quando necessaria.\n\n"
            "12. Desempenho\nConsulte metrics.csv.\n\n"
            "13. Avisos e limitacoes\n%s\n",
            result->actual_run_name,
            config->homeostasis_target_rate,
            config->homeostasis_update_interval_steps,
            data->stats.final_population_rate,
            data->stats.threshold_increase_events +
                data->stats.threshold_decrease_events,
            data->stats.scaling_events,
            data->stats.final_inhibitory_gain,
            config->homeostasis_threshold_min,
            config->homeostasis_threshold_max,
            text) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    if (fclose(file) != 0)
        return 0;

    if (!make_path(path, result->output_directory, "homeostasis_report.html"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\">"
            "<title>miniSNN - Homeostase</title><body>"
            "<h1>Relatorio de homeostase</h1>"
            "<p><strong>Execucao:</strong> %s</p>"
            "<p>Taxa-alvo: %.17g; taxa final observada: %.17g.</p>"
            "<p>Threshold efetivo adaptado em %llu neuronios; ganho inibitorio final %.17g.</p>"
            "<p>Eventos de scaling: %llu.</p><p>%s</p>"
            "<p><a href=\"homeostasis_metrics.csv\">Metricas CSV</a> | "
            "<a href=\"homeostasis_history.csv\">Historico</a></p>"
            "</body></html>\n",
            result->actual_run_name,
            config->homeostasis_target_rate,
            data->stats.final_population_rate,
            data->stats.threshold_modified_neuron_count,
            data->stats.final_inhibitory_gain,
            data->stats.scaling_events,
            text) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int homeostasis_run_data_finalize(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    HomeostasisRunData *data)
{
    if (!data->enabled)
        return 1;

    if (data->history_file != NULL && data->last_history_step != config->steps &&
        !write_homeostasis_history_step(snn, config, data, config->steps))
    {
        return 0;
    }

    if (data->history_file != NULL && fclose(data->history_file) != 0)
        return 0;
    data->history_file = NULL;
    if (data->threshold_file != NULL && fclose(data->threshold_file) != 0)
        return 0;
    data->threshold_file = NULL;

    return write_homeostasis_neurons(snn, config, result, data) &&
        write_homeostasis_metrics(snn, config, result, data) &&
        write_homeostasis_reports(config, result, data);
}

static void reward_run_data_close(RewardRunData *data)
{
    if (data == NULL)
        return;

    if (data->events_file != NULL)
        fclose(data->events_file);
    if (data->history_file != NULL)
        fclose(data->history_file);
    if (data->eligibility_file != NULL)
        fclose(data->eligibility_file);
    free(data->sample_ids);
    free(data->initial_weights);
    memset(data, 0, sizeof(*data));
}

static int write_reward_history_step(
    const MiniSNN *snn,
    RewardRunData *data,
    int step)
{
    MiniSNNRewardStats stats;
    double pending_reward;
    double last_applied_reward;

    if (data->history_file == NULL || data->eligibility_file == NULL)
        return 1;
    if (data->last_history_step == step)
        return 1;

    if (!minisnn_get_reward_stats(snn, &stats) ||
        !minisnn_get_pending_reward(snn, &pending_reward) ||
        !minisnn_get_last_applied_reward(snn, &last_applied_reward) ||
        fprintf(
            data->history_file,
            "%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%zu,%.17g,%.17g\n",
            step,
            pending_reward,
            last_applied_reward,
            stats.cumulative_applied_reward,
            stats.cumulative_absolute_reward,
            stats.eligibility_final_mean,
            stats.eligibility_final_min,
            stats.eligibility_final_max,
            stats.eligibility_final_mean_absolute,
            stats.modified_connection_count,
            stats.total_signed_weight_change,
            stats.total_absolute_weight_change) < 0)
    {
        return 0;
    }

    for (size_t i = 0; i < data->sample_count; i++)
    {
        size_t connection_id = data->sample_ids[i];
        MiniSNNConnectionInfo connection;
        MiniSNNRewardConnectionStats reward_connection;

        if (!minisnn_get_connection(snn, connection_id, &connection) ||
            !minisnn_get_reward_connection_stats(
                snn, connection_id, &reward_connection) ||
            fprintf(
                data->eligibility_file,
                "%d,%zu,%zu,%zu,%.17g,1\n",
                step,
                connection_id,
                connection.source,
                connection.target,
                reward_connection.eligibility) < 0)
        {
            return 0;
        }
    }

    data->last_history_step = step;
    return 1;
}

static int reward_run_data_prepare(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const char *output_dir,
    RewardRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    size_t *eligible_ids = NULL;
    size_t eligible_count = 0;

    memset(data, 0, sizeof(*data));
    data->enabled = config->reward_enabled;
    data->last_history_step = -1;
    data->first_event_step = -1;
    data->last_event_step = -1;

    if (!data->enabled)
        return 1;

    data->total_connections = minisnn_connection_count(snn);
    if (data->total_connections > 0)
    {
        data->initial_weights = malloc(
            data->total_connections * sizeof(*data->initial_weights));
        eligible_ids = malloc(
            data->total_connections * sizeof(*eligible_ids));
        if (data->initial_weights == NULL || eligible_ids == NULL)
            goto fail;
    }

    for (size_t i = 0; i < data->total_connections; i++)
    {
        MiniSNNRewardConnectionStats reward_connection;

        if (!minisnn_get_connection_weight(snn, i, &data->initial_weights[i]) ||
            !minisnn_get_reward_connection_stats(
                snn, i, &reward_connection))
        {
            goto fail;
        }
        if (reward_connection.eligible)
            eligible_ids[eligible_count++] = i;
    }

    data->sample_count = eligible_count <
            (size_t)config->reward_record_connection_limit ?
        eligible_count : (size_t)config->reward_record_connection_limit;
    if (data->sample_count > 0)
    {
        data->sample_ids = malloc(
            data->sample_count * sizeof(*data->sample_ids));
        if (data->sample_ids == NULL)
            goto fail;

        for (size_t i = 0; i < data->sample_count; i++)
        {
            size_t position = distributed_sample_id(
                eligible_count,
                data->sample_count,
                i);
            data->sample_ids[i] = eligible_ids[position];
        }
    }
    free(eligible_ids);
    eligible_ids = NULL;

    for (int i = 0; i < config->reward_event_count; i++)
    {
        int step = config->reward_events[i].step;

        if (data->first_event_step < 0 || step < data->first_event_step)
            data->first_event_step = step;
        if (step > data->last_event_step)
            data->last_event_step = step;
        if (i == 0 || step != config->reward_events[i - 1].step)
            data->scheduled_step_count++;
    }

    if (!make_path(path, output_dir, "reward_events.csv"))
        goto fail;
    data->events_file = fopen(path, "w");
    if (data->events_file == NULL || fprintf(
            data->events_file,
            "step,raw_reward,applied_reward,event_component_count,active_eligibility_count,modified_connection_count,weight_signed_change,weight_absolute_change,weight_clamp_min_count,weight_clamp_max_count\n") < 0)
    {
        goto fail;
    }

    if (!make_path(path, output_dir, "reward_history.csv"))
        goto fail;
    data->history_file = fopen(path, "w");
    if (data->history_file == NULL || fprintf(
            data->history_file,
            "step,pending_reward,last_applied_reward,cumulative_reward,cumulative_absolute_reward,eligibility_mean,eligibility_min,eligibility_max,eligibility_mean_absolute,modified_connection_count_cumulative,weight_signed_change_cumulative,weight_absolute_change_cumulative\n") < 0)
    {
        goto fail;
    }

    if (!make_path(path, output_dir, "eligibility_history.csv"))
        goto fail;
    data->eligibility_file = fopen(path, "w");
    if (data->eligibility_file == NULL || fprintf(
            data->eligibility_file,
            "step,connection_id,source,target,eligibility,sampled\n") < 0 ||
        !write_reward_history_step(snn, data, 0))
    {
        goto fail;
    }

    return 1;

fail:
    free(eligible_ids);
    reward_run_data_close(data);
    return 0;
}

static int write_reward_event_step(
    const MiniSNN *snn,
    RewardRunData *data,
    int step,
    int event_component_count)
{
    MiniSNNRewardStats stats;

    if (data->events_file == NULL ||
        !minisnn_get_reward_stats(snn, &stats))
    {
        return 0;
    }

    return fprintf(
        data->events_file,
        "%d,%.17g,%.17g,%u,%zu,%zu,%.17g,%.17g,%llu,%llu\n",
        step,
        stats.last_raw_reward,
        stats.last_applied_reward,
        (unsigned int)event_component_count,
        stats.last_active_eligibility_count,
        stats.last_modified_connection_count,
        stats.last_weight_signed_change,
        stats.last_weight_absolute_change,
        stats.last_weight_clamp_min_count,
        stats.last_weight_clamp_max_count) >= 0;
}

static int write_reward_connections(
    const MiniSNN *snn,
    const ScenarioRunResult *result,
    const RewardRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;

    if (!make_path(path, result->output_directory, "reward_connections.csv"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "connection_id,source,target,source_type,target_type,eligible,sampled,initial_weight,final_weight,net_weight_change,final_eligibility,max_absolute_eligibility,reward_update_count,reward_signed_change,reward_absolute_change\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    for (size_t i = 0; i < data->total_connections; i++)
    {
        MiniSNNConnectionInfo connection;
        MiniSNNRewardConnectionStats reward_connection;
        int sampled = 0;

        for (size_t j = 0; j < data->sample_count; j++)
        {
            if (data->sample_ids[j] == i)
            {
                sampled = 1;
                break;
            }
        }

        if (!minisnn_get_connection(snn, i, &connection) ||
            !minisnn_get_reward_connection_stats(
                snn, i, &reward_connection) ||
            fprintf(
                file,
                "%zu,%zu,%zu,%s,%s,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%.17g,%.17g\n",
                i,
                connection.source,
                connection.target,
                connection.source_type == MINISNN_NEURON_INHIBITORY ?
                    "INH" : "EXC",
                connection.target_type == MINISNN_NEURON_INHIBITORY ?
                    "INH" : "EXC",
                reward_connection.eligible,
                sampled,
                data->initial_weights[i],
                connection.weight,
                connection.weight - data->initial_weights[i],
                reward_connection.eligibility,
                reward_connection.max_absolute_eligibility,
                reward_connection.reward_update_count,
                reward_connection.reward_signed_change,
                reward_connection.reward_absolute_change) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

static int write_reward_metrics(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    RewardRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    double modified_fraction;
    double mean_absolute_reward;
    double mean_signed_change;
    double mean_absolute_change;

    if (!minisnn_get_reward_stats(snn, &data->stats))
        return 0;

    modified_fraction = data->stats.eligible_connection_count > 0 ?
        (double)data->stats.modified_connection_count /
            (double)data->stats.eligible_connection_count : 0.0;
    mean_absolute_reward = data->stats.reward_event_count > 0 ?
        data->stats.cumulative_absolute_reward /
            (double)data->stats.reward_event_count : 0.0;
    mean_signed_change = data->stats.modified_connection_count > 0 ?
        data->stats.total_signed_weight_change /
            (double)data->stats.modified_connection_count : 0.0;
    mean_absolute_change = data->stats.modified_connection_count > 0 ?
        data->stats.total_absolute_weight_change /
            (double)data->stats.modified_connection_count : 0.0;

    if (!make_path(path, result->output_directory, "reward_metrics.csv"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "reward_enabled,reward_mode,reward_learning_rate,reward_eligibility_tau,reward_eligibility_min,reward_eligibility_max,reward_signal_min,reward_signal_max,reward_clip_enabled,reward_event_count,reward_positive_event_count,reward_negative_event_count,reward_zero_event_count,reward_cumulative_raw,reward_cumulative_applied,reward_cumulative_positive,reward_cumulative_negative,reward_cumulative_absolute,reward_mean_absolute,reward_last_raw,reward_last_applied,reward_eligible_connection_count,reward_modified_connection_count,reward_modified_connection_fraction,reward_eligibility_final_mean,reward_eligibility_final_min,reward_eligibility_final_max,reward_eligibility_final_mean_absolute,reward_eligibility_max_absolute_observed,reward_eligibility_potentiation_events,reward_eligibility_depression_events,reward_eligibility_clamp_min_events,reward_eligibility_clamp_max_events,reward_weight_potentiation_events,reward_weight_depression_events,reward_weight_total_signed_change,reward_weight_total_absolute_change,reward_weight_mean_signed_change,reward_weight_mean_absolute_change,reward_weight_max_absolute_change,reward_weight_clamp_min_events,reward_weight_clamp_max_events,reward_scheduled_event_count,reward_scheduled_step_count,reward_first_event_step,reward_last_event_step\n"
            "true,rstdp,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%s,%llu,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%zu,%zu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%llu,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu,%d,%d,%d,%d\n",
            config->reward_learning_rate,
            config->reward_eligibility_tau,
            config->reward_eligibility_min,
            config->reward_eligibility_max,
            config->reward_min,
            config->reward_max,
            config->reward_clip ? "true" : "false",
            data->stats.reward_event_count,
            data->stats.positive_reward_event_count,
            data->stats.negative_reward_event_count,
            data->stats.zero_reward_event_count,
            data->stats.cumulative_raw_reward,
            data->stats.cumulative_applied_reward,
            data->stats.cumulative_positive_reward,
            data->stats.cumulative_negative_reward,
            data->stats.cumulative_absolute_reward,
            mean_absolute_reward,
            data->stats.last_raw_reward,
            data->stats.last_applied_reward,
            data->stats.eligible_connection_count,
            data->stats.modified_connection_count,
            modified_fraction,
            data->stats.eligibility_final_mean,
            data->stats.eligibility_final_min,
            data->stats.eligibility_final_max,
            data->stats.eligibility_final_mean_absolute,
            data->stats.eligibility_max_absolute_observed,
            data->stats.eligibility_potentiation_events,
            data->stats.eligibility_depression_events,
            data->stats.eligibility_clamp_min_events,
            data->stats.eligibility_clamp_max_events,
            data->stats.reward_potentiation_events,
            data->stats.reward_depression_events,
            data->stats.total_signed_weight_change,
            data->stats.total_absolute_weight_change,
            mean_signed_change,
            mean_absolute_change,
            data->stats.max_absolute_weight_change,
            data->stats.weight_clamp_min_events,
            data->stats.weight_clamp_max_events,
            config->reward_event_count,
            data->scheduled_step_count,
            data->first_event_step,
            data->last_event_step) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int write_reward_reports(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const RewardRunData *data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    FILE *file;
    const char *limitation =
        "A mudanca nao prova aprendizado de uma tarefa; o reward foi fornecido "
        "externamente e nao existe previsao de recompensa.";

    if (!make_path(path, result->output_directory, "reward_report.txt"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "MINISNN - RELATORIO DE RECOMPENSA E PUNICAO\n\n"
            "1. Identificacao da execucao\nrun_name=%s\n\n"
            "2. Configuracao de aprendizado\nmode=reward_modulated_stdp learning_rate=%.17g\n\n"
            "3. Convencao temporal\nO evento do step k e enfileirado antes do step; elegibilidade e atualizada antes do reward.\n\n"
            "4. Eventos de recompensa\n%llu\n\n"
            "5. Eventos de punicao\n%llu\n\n"
            "6. Elegibilidades\nmedia_final=%.17g max_abs=%.17g\n\n"
            "7. Alteracoes de peso por reward\nsigned=%.17g absolute=%.17g\n\n"
            "8. Relacao com STDP\nA correlacao temporal gera elegibilidade; nao altera diretamente o peso.\n\n"
            "9. Relacao com homeostase\nR-STDP ocorre antes do scaling homeostatico. homeostasis=%s\n\n"
            "10. Clamps e limites\nweight_min=%.17g weight_max=%.17g clamp_min_events=%llu clamp_max_events=%llu\n\n"
            "11. Conexoes mais afetadas\nConsulte reward_connections.csv.\n\n"
            "12. Amostragem\n%zu de %zu conexoes elegiveis.\n\n"
            "13. Desempenho\nConsulte metrics.csv.\n\n"
            "14. Avisos e limitacoes\n%s\n",
            result->actual_run_name,
            config->reward_learning_rate,
            data->stats.positive_reward_event_count,
            data->stats.negative_reward_event_count,
            data->stats.eligibility_final_mean,
            data->stats.eligibility_max_absolute_observed,
            data->stats.total_signed_weight_change,
            data->stats.total_absolute_weight_change,
            config->homeostasis_enabled ? "ON" : "OFF",
            config->plasticity_weight_min,
            config->plasticity_weight_max,
            data->stats.weight_clamp_min_events,
            data->stats.weight_clamp_max_events,
            data->sample_count,
            data->stats.eligible_connection_count,
            limitation) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    if (fclose(file) != 0)
        return 0;

    if (!make_path(path, result->output_directory, "reward_report.html"))
        return 0;
    file = fopen(path, "w");
    if (file == NULL || fprintf(
            file,
            "<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\">"
            "<title>miniSNN - Recompensa</title><body>"
            "<h1>Relatorio de recompensa e punicao</h1>"
            "<p><strong>Execucao:</strong> %s</p>"
            "<p>Reward aplicado: %.17g; punicao acumulada: %.17g.</p>"
            "<p>Conexoes modificadas: %zu; mudanca total: %.17g.</p>"
            "<p>Modo STDP: reward_modulated_stdp; homeostase: %s.</p>"
            "<p>%s</p><p><a href=\"reward_metrics.csv\">Metricas</a> | "
            "<a href=\"reward_events.csv\">Eventos</a> | "
            "<a href=\"reward_connections.csv\">Conexoes</a></p>"
            "</body></html>\n",
            result->actual_run_name,
            data->stats.cumulative_positive_reward,
            data->stats.cumulative_negative_reward,
            data->stats.modified_connection_count,
            data->stats.total_signed_weight_change,
            config->homeostasis_enabled ? "ON" : "OFF",
            limitation) < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int reward_run_data_finalize(
    const MiniSNN *snn,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    RewardRunData *data)
{
    if (!data->enabled)
        return 1;

    if (data->last_history_step != config->steps &&
        !write_reward_history_step(snn, data, config->steps))
    {
        return 0;
    }

    if (data->events_file != NULL && fclose(data->events_file) != 0)
        return 0;
    data->events_file = NULL;
    if (data->history_file != NULL && fclose(data->history_file) != 0)
        return 0;
    data->history_file = NULL;
    if (data->eligibility_file != NULL && fclose(data->eligibility_file) != 0)
        return 0;
    data->eligibility_file = NULL;

    return write_reward_connections(snn, result, data) &&
        write_reward_metrics(snn, config, result, data) &&
        write_reward_reports(config, result, data);
}

static int open_output_files(
    const ScenarioConfig *config,
    const char *output_dir,
    FILE **population_file,
    FILE **raster_file,
    FILE **neuron_file,
    FILE **summary_file,
    char *population_path,
    char *raster_path,
    char *neuron_path,
    char *summary_path)
{
    char neuron_filename[64];

    if (!make_path(population_path, output_dir, "population.csv") ||
        !make_path(raster_path, output_dir, "raster.csv") ||
        !make_path(summary_path, output_dir, "summary.txt"))
    {
        return 0;
    }

    if (snprintf(
            neuron_filename,
            sizeof(neuron_filename),
            "neuron_%d.csv",
            config->record_neuron) >= (int)sizeof(neuron_filename))
    {
        return 0;
    }

    if (!make_path(neuron_path, output_dir, neuron_filename))
        return 0;

    *population_file = fopen(population_path, "w");
    *raster_file = fopen(raster_path, "w");
    *neuron_file = fopen(neuron_path, "w");
    *summary_file = fopen(summary_path, "w");

    if (*population_file == NULL ||
        *raster_file == NULL ||
        *neuron_file == NULL ||
        *summary_file == NULL)
    {
        return 0;
    }

    if (fprintf(
            *population_file,
            "tempo,spikes_total,spikes_exc,spikes_inh,mean_potential,mean_syn_current\n") < 0)
    {
        return 0;
    }

    if (fprintf(*raster_file, "tempo,neuronio,tipo\n") < 0)
        return 0;

    if (fprintf(
            *neuron_file,
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n") < 0)
    {
        return 0;
    }

    return 1;
}

static void close_file_if_open(FILE *file)
{
    if (file != NULL)
        fclose(file);
}

static int write_summary(
    FILE *file,
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const PlasticityRunData *plasticity_data,
    const HomeostasisRunData *homeostasis_data,
    const RewardRunData *reward_data)
{
    return fprintf(
               file,
               "run_name=%s\n"
               "actual_run_name=%s\n"
               "topology=%s\n"
               "neurons=%d\n"
               "inhibitory_count=%d\n"
               "connection_count=%d\n"
               "self_connection_count=%d\n"
               "excitatory_to_excitatory_count=%d\n"
               "excitatory_to_inhibitory_count=%d\n"
               "inhibitory_to_excitatory_count=%d\n"
               "inhibitory_to_inhibitory_count=%d\n"
               "topology_signature=%016llx\n"
               "steps=%d\n"
               "input_current=%.6f\n"
               "source_count=%d\n"
               "spikes_total=%d\n"
               "spikes_exc=%d\n"
               "spikes_inh=%d\n"
               "first_active_step=%d\n"
               "last_active_step=%d\n"
               "plasticity_enabled=%s\n"
               "plasticity_rule=%s\n"
               "plasticity_learning_mode=%s\n"
               "plasticity_eligible_connections=%llu\n"
               "plasticity_modified_connections=%llu\n"
               "plasticity_total_signed_change=%.17g\n"
               "homeostasis_enabled=%s\n"
               "homeostasis_update_count=%llu\n"
               "homeostasis_population_rate_final=%.17g\n"
               "homeostasis_inhibitory_gain_final=%.17g\n"
               "reward_enabled=%s\n"
               "reward_mode=%s\n"
               "reward_event_count=%llu\n"
               "reward_cumulative_applied=%.17g\n"
               "reward_modified_connections=%zu\n"
               "reward_weight_total_signed_change=%.17g\n",
               config->run_name,
               result->actual_run_name,
               config->topology,
               config->neurons,
               result->inhibitory_count,
               result->connection_count,
               result->self_connection_count,
               result->excitatory_to_excitatory_count,
               result->excitatory_to_inhibitory_count,
               result->inhibitory_to_excitatory_count,
               result->inhibitory_to_inhibitory_count,
               result->topology_signature,
               config->steps,
               config->input_current,
               config->source_count,
               result->spikes_total,
               result->spikes_exc,
               result->spikes_inh,
               result->first_active_step,
               result->last_active_step,
               config->plasticity_enabled ? "true" : "false",
               config->plasticity_rule,
               config->plasticity_learning_mode,
               (unsigned long long)plasticity_data->stats.eligible_connections,
               (unsigned long long)plasticity_data->stats.modified_connections,
               plasticity_data->stats.total_signed_change,
               config->homeostasis_enabled ? "true" : "false",
               homeostasis_data->stats.update_count,
               homeostasis_data->stats.final_population_rate,
               config->homeostasis_enabled ?
                   homeostasis_data->stats.final_inhibitory_gain : 1.0,
               config->reward_enabled ? "true" : "false",
               config->reward_mode,
               reward_data->stats.reward_event_count,
               reward_data->stats.cumulative_applied_reward,
               reward_data->stats.modified_connection_count,
               reward_data->stats.total_signed_weight_change) >= 0;
}

static int append_scenario_history(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const char *source_config_path,
    const char *status)
{
    int needs_header = !file_exists(SCENARIO_HISTORY_PATH);
    FILE *file;
    char timestamp[32];

    if (!needs_header)
    {
        char header[512];

        file = fopen(SCENARIO_HISTORY_PATH, "r");
        if (file == NULL)
            return 0;

        if (fgets(header, sizeof(header), file) == NULL ||
            strcmp(header, SCENARIO_HISTORY_HEADER) != 0)
        {
            fclose(file);
            return 0;
        }

        fclose(file);
    }

    file = fopen(SCENARIO_HISTORY_PATH, "a");

    if (file == NULL)
        return 0;

    if (needs_header)
    {
        if (fprintf(
                file,
                SCENARIO_HISTORY_HEADER) < 0)
        {
            fclose(file);
            return 0;
        }
    }

    current_timestamp(timestamp, sizeof(timestamp));

    if (fprintf(
            file,
            "%s,%s,%s,%s,%s,%s,%d,%d,%.12g,%u,%d,%d,%d,%d,%d,%s\n",
            timestamp,
            config->run_name,
            result->actual_run_name,
            result->output_directory,
            source_config_path != NULL ? source_config_path : "NA",
            config->topology,
            config->neurons,
            config->steps,
            config->dt,
            config->seed,
            config->record_neuron,
            result->connection_count,
            result->spikes_total,
            result->first_active_step,
            result->last_active_step,
            status) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static double nonnegative_sqrt(double value)
{
    return value > 0.0 ? sqrt(value) : 0.0;
}

static void degree_statistics(
    const int *values,
    int count,
    double *mean,
    int *minimum,
    int *maximum,
    double *stddev)
{
    double sum = 0.0;
    double square_sum = 0.0;

    *minimum = 0;
    *maximum = 0;
    *mean = 0.0;
    *stddev = 0.0;

    if (count <= 0)
        return;

    *minimum = values[0];
    *maximum = values[0];

    for (int i = 0; i < count; i++)
    {
        double value = (double)values[i];
        sum += value;
        square_sum += value * value;

        if (values[i] < *minimum)
            *minimum = values[i];
        if (values[i] > *maximum)
            *maximum = values[i];
    }

    *mean = sum / (double)count;
    *stddev = nonnegative_sqrt(
        square_sum / (double)count - (*mean * *mean));
}

static unsigned long long file_size_bytes(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    ULARGE_INTEGER size;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
        return 0ULL;

    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart;
}

static int write_basic_metrics(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const ConnectivityStats *connectivity,
    double simulation_seconds,
    double wall_seconds,
    const char *population_path,
    const char *raster_path,
    const PlasticityRunData *plasticity_data,
    const HomeostasisRunData *homeostasis_data,
    const RewardRunData *reward_data)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    char timestamp[32];
    char homeostasis_metrics[512] = "";
    const char *homeostasis_header = "";
    char reward_metrics[768] = "";
    const char *reward_header = "";
    FILE *file;
    double activity_fraction;
    double silence_fraction;
    double connection_density;
    double weight_mean = 0.0;
    double weight_std = 0.0;
    double delay_mean = 0.0;
    double delay_std = 0.0;
    double indegree_mean;
    double indegree_std;
    double outdegree_mean;
    double outdegree_std;
    int indegree_min;
    int indegree_max;
    int outdegree_min;
    int outdegree_max;
    int possible_connections;
    int has_late_activity;
    double plasticity_modified_fraction =
        plasticity_data->stats.eligible_connections > 0 ?
        (double)plasticity_data->stats.modified_connections /
            (double)plasticity_data->stats.eligible_connections :
        0.0;
    double plasticity_mean_absolute_change =
        plasticity_data->stats.eligible_connections > 0 ?
        plasticity_data->stats.total_absolute_change /
            (double)plasticity_data->stats.eligible_connections :
        0.0;

    if (!make_path(path, result->output_directory, "metrics.csv"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    activity_fraction = (double)result->active_timesteps / (double)config->steps;
    silence_fraction = 1.0 - activity_fraction;
    possible_connections = config->allow_self_connections ?
        config->neurons * config->neurons :
        config->neurons * (config->neurons - 1);
    connection_density = possible_connections > 0 ?
        (double)connectivity->count / (double)possible_connections :
        0.0;
    has_late_activity = result->last_active_step >=
        config->steps - (config->steps + 3) / 4;

    if (connectivity->count > 0)
    {
        weight_mean = connectivity->weight_sum / (double)connectivity->count;
        weight_std = nonnegative_sqrt(
            connectivity->weight_square_sum / (double)connectivity->count -
            weight_mean * weight_mean);
        delay_mean = connectivity->delay_sum / (double)connectivity->count;
        delay_std = nonnegative_sqrt(
            connectivity->delay_square_sum / (double)connectivity->count -
            delay_mean * delay_mean);
    }

    degree_statistics(
        connectivity->indegree,
        config->neurons,
        &indegree_mean,
        &indegree_min,
        &indegree_max,
        &indegree_std);
    degree_statistics(
        connectivity->outdegree,
        config->neurons,
        &outdegree_mean,
        &outdegree_min,
        &outdegree_max,
        &outdegree_std);
    current_timestamp(timestamp, sizeof(timestamp));

    if (config->homeostasis_enabled)
    {
        int written;

        homeostasis_header =
            ",homeostasis_enabled,homeostasis_intrinsic_enabled,"
            "homeostasis_synaptic_scaling_enabled,"
            "homeostasis_inhibitory_gain_enabled,homeostasis_target_rate,"
            "homeostasis_population_rate_final,homeostasis_rate_error_final,"
            "homeostasis_threshold_final_mean,homeostasis_scaling_events,"
            "homeostasis_inhibitory_gain_final";
        written = snprintf(
            homeostasis_metrics,
            sizeof(homeostasis_metrics),
            ",true,%s,%s,%s,%.17g,%.17g,%.17g,%.17g,%llu,%.17g",
            config->homeostasis_intrinsic_enabled ? "true" : "false",
            config->homeostasis_synaptic_scaling_enabled ? "true" : "false",
            config->homeostasis_inhibitory_gain_enabled ? "true" : "false",
            config->homeostasis_target_rate,
            homeostasis_data->stats.final_population_rate,
            homeostasis_data->stats.final_population_rate -
                config->homeostasis_target_rate,
            homeostasis_data->final_threshold_mean,
            homeostasis_data->stats.scaling_events,
            homeostasis_data->stats.final_inhibitory_gain);
        if (written < 0 || (size_t)written >= sizeof(homeostasis_metrics))
        {
            fclose(file);
            return 0;
        }
    }

    if (config->reward_enabled)
    {
        int written;
        double modified_fraction =
            reward_data->stats.eligible_connection_count > 0 ?
            (double)reward_data->stats.modified_connection_count /
                (double)reward_data->stats.eligible_connection_count : 0.0;
        double mean_absolute_change =
            reward_data->stats.modified_connection_count > 0 ?
            reward_data->stats.total_absolute_weight_change /
                (double)reward_data->stats.modified_connection_count : 0.0;

        reward_header =
            ",reward_enabled,reward_learning_mode,reward_event_count,"
            "reward_positive_event_count,reward_negative_event_count,"
            "reward_cumulative_applied,reward_cumulative_absolute,"
            "reward_modified_connection_fraction,"
            "reward_eligibility_final_mean_absolute,"
            "reward_eligibility_max_absolute_observed,"
            "reward_weight_total_signed_change,"
            "reward_weight_total_absolute_change,"
            "reward_weight_mean_absolute_change";
        written = snprintf(
            reward_metrics,
            sizeof(reward_metrics),
            ",true,%s,%llu,%llu,%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g",
            config->plasticity_learning_mode,
            reward_data->stats.reward_event_count,
            reward_data->stats.positive_reward_event_count,
            reward_data->stats.negative_reward_event_count,
            reward_data->stats.cumulative_applied_reward,
            reward_data->stats.cumulative_absolute_reward,
            modified_fraction,
            reward_data->stats.eligibility_final_mean_absolute,
            reward_data->stats.eligibility_max_absolute_observed,
            reward_data->stats.total_signed_weight_change,
            reward_data->stats.total_absolute_weight_change,
            mean_absolute_change);
        if (written < 0 || (size_t)written >= sizeof(reward_metrics))
        {
            fclose(file);
            return 0;
        }
    }

    if (fprintf(
            file,
            "run_name,actual_run_name,run_path,timestamp,topology,network_num_neurons,run_steps,run_dt,run_simulation_duration,run_seed,run_recorded_neuron,diagnostics_level,network_total_connections,network_excitatory_neuron_count,network_inhibitory_neuron_count,activity_total_spikes,activity_mean_spikes_per_step,activity_min_spikes_per_step,activity_max_spikes_per_step,activity_active_timesteps,activity_silent_timesteps,activity_fraction,silence_fraction,activity_first_active_step,activity_last_active_step,activity_peak_step,activity_peak_value,activity_has_late_activity,neuron_mean_spikes,exc_total_spikes,inh_total_spikes,network_connection_density,network_self_connection_count,network_excitatory_connection_count,network_inhibitory_connection_count,network_weight_mean,network_weight_min,network_weight_max,network_weight_std,network_delay_mean,network_delay_min,network_delay_max,network_delay_std,network_indegree_mean,network_indegree_min,network_indegree_max,network_indegree_std,network_outdegree_mean,network_outdegree_min,network_outdegree_max,network_outdegree_std,performance_simulation_time_seconds,performance_wall_time_seconds,performance_steps_per_second,performance_neuron_updates_per_second,performance_spikes_per_second_processed,performance_population_csv_size_bytes,performance_raster_csv_size_bytes,plasticity_enabled,plasticity_modified_connection_fraction,plasticity_initial_weight_mean,plasticity_final_weight_mean,plasticity_mean_absolute_change,plasticity_total_signed_change,plasticity_potentiation_events,plasticity_depression_events%s%s\n",
            homeostasis_header,
            reward_header) < 0 ||
        fprintf(
            file,
            "%s,%s,%s,%s,%s,%d,%d,%.12g,%.12g,%u,%d,%s,%d,%d,%d,%d,%.12g,%d,%d,%d,%d,%.12g,%.12g,%d,%d,%d,%d,%s,%.12g,%d,%d,%.12g,%d,%d,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%d,%.12g,%.12g,%d,%d,%.12g,%.12g,%d,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%llu,%llu,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%llu%s%s\n",
            config->run_name,
            result->actual_run_name,
            result->output_directory,
            timestamp,
            config->topology,
            config->neurons,
            config->steps,
            config->dt,
            config->steps * config->dt,
            config->seed,
            config->record_neuron,
            config->diagnostics_level,
            connectivity->count,
            config->neurons - result->inhibitory_count,
            result->inhibitory_count,
            result->spikes_total,
            (double)result->spikes_total / (double)config->steps,
            result->min_activity_value,
            result->peak_activity_value,
            result->active_timesteps,
            config->steps - result->active_timesteps,
            activity_fraction,
            silence_fraction,
            result->first_active_step,
            result->last_active_step,
            result->peak_activity_step,
            result->peak_activity_value,
            has_late_activity ? "true" : "false",
            (double)result->spikes_total / (double)config->neurons,
            result->spikes_exc,
            result->spikes_inh,
            connection_density,
            connectivity->self_count,
            connectivity->excitatory_count,
            connectivity->inhibitory_count,
            weight_mean,
            connectivity->count > 0 ? connectivity->weight_min : 0.0,
            connectivity->count > 0 ? connectivity->weight_max : 0.0,
            weight_std,
            delay_mean,
            connectivity->count > 0 ? connectivity->delay_min : 0,
            connectivity->count > 0 ? connectivity->delay_max : 0,
            delay_std,
            indegree_mean,
            indegree_min,
            indegree_max,
            indegree_std,
            outdegree_mean,
            outdegree_min,
            outdegree_max,
            outdegree_std,
            simulation_seconds,
            wall_seconds,
            simulation_seconds > 0.0 ? config->steps / simulation_seconds : 0.0,
            simulation_seconds > 0.0 ?
                ((double)config->steps * config->neurons) / simulation_seconds : 0.0,
            simulation_seconds > 0.0 ? result->spikes_total / simulation_seconds : 0.0,
            file_size_bytes(population_path),
            file_size_bytes(raster_path),
            config->plasticity_enabled ? "true" : "false",
            plasticity_modified_fraction,
            weight_aggregate_mean(&plasticity_data->initial_weights),
            weight_aggregate_mean(&plasticity_data->final_weights),
            plasticity_mean_absolute_change,
            plasticity_data->stats.total_signed_change,
            plasticity_data->stats.potentiation_events,
            plasticity_data->stats.depression_events,
            homeostasis_metrics,
            reward_metrics) < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int write_run_manifest(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const char *source_config_path,
    int metrics_generated)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    char timestamp[32];
    FILE *file;
    char git_commit[96] = "NA";
    char git_status[32] = "NA";
    char plasticity_files[256] = "NA";
    char homeostasis_files[384] = "NA";
    char reward_files[384] = "NA";
    FILE *pipe;

    if (!make_path(path, result->output_directory, "run_manifest.txt"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    current_timestamp(timestamp, sizeof(timestamp));

    if (config->plasticity_enabled)
    {
        snprintf(
            plasticity_files,
            sizeof(plasticity_files),
            "plasticity_metrics.csv;stdp_report.txt%s%s",
            config->plasticity_record_weights ?
                ";weights_initial.csv;weights_final.csv" :
                "",
            config->plasticity_record_history ?
                ";weight_history.csv" :
                "");
    }

    if (config->homeostasis_enabled)
    {
        snprintf(
            homeostasis_files,
            sizeof(homeostasis_files),
            "homeostasis_metrics.csv;homeostasis_neurons.csv;homeostasis_report.txt;homeostasis_report.html;homeostasis_history.csv;threshold_history.csv");
    }

    if (config->reward_enabled)
    {
        snprintf(
            reward_files,
            sizeof(reward_files),
            "reward_metrics.csv;reward_events.csv;reward_history.csv;eligibility_history.csv;reward_connections.csv;reward_report.txt;reward_report.html");
    }

    pipe = _popen("git rev-parse --short HEAD 2>NUL", "r");
    if (pipe != NULL)
    {
        if (fgets(git_commit, sizeof(git_commit), pipe) != NULL)
        {
            git_commit[strcspn(git_commit, "\r\n")] = '\0';
            if (_pclose(pipe) != 0)
                snprintf(git_commit, sizeof(git_commit), "NA");
        }
        else
        {
            _pclose(pipe);
            snprintf(git_commit, sizeof(git_commit), "NA");
        }
    }

    pipe = _popen("git status --porcelain 2>NUL", "r");
    if (pipe != NULL)
    {
        char status_line[8];
        int has_changes = fgets(status_line, sizeof(status_line), pipe) != NULL;
        if (_pclose(pipe) == 0)
            snprintf(git_status, sizeof(git_status), "%s", has_changes ? "dirty" : "clean");
    }

    if (fprintf(
            file,
            "miniSNN_version=%s\n"
            "git_commit=%s\n"
            "git_status=%s\n"
            "timestamp=%s\n"
            "operating_system=Windows\n"
            "compiler=gcc\n"
            "compiler_version=%s\n"
            "architecture=%s\n"
            "original_config=%s\n"
            "effective_config=%s/config_used.ini\n"
            "requested_run_name=%s\n"
            "actual_run_name=%s\n"
            "seed=%u\n"
            "neural_model=LIF\n"
            "num_neurons=%d\n"
            "topology=%s\n"
            "dt=%.12g\n"
            "steps=%d\n"
            "diagnostics_level=%s\n"
            "plasticity_enabled=%s\n"
            "plasticity_rule=%s\n"
            "plasticity_learning_mode=%s\n"
            "plasticity_a_plus=%.17g\n"
            "plasticity_a_minus=%.17g\n"
            "plasticity_tau_plus=%.17g\n"
            "plasticity_tau_minus=%.17g\n"
            "plasticity_trace_increment=%.17g\n"
            "plasticity_weight_min=%.17g\n"
            "plasticity_weight_max=%.17g\n"
            "plasticity_timing_reference=spike_emission\n"
            "inhibitory_plasticity=disabled\n"
            "plasticity_record_weights=%s\n"
            "plasticity_record_history=%s\n"
            "plasticity_record_interval_steps=%d\n"
            "plasticity_record_connection_limit=%d\n"
            "plasticity_output_files=%s\n"
            "homeostasis_enabled=%s\n"
            "homeostasis_intrinsic_enabled=%s\n"
            "homeostasis_synaptic_scaling_enabled=%s\n"
            "homeostasis_inhibitory_gain_enabled=%s\n"
            "homeostasis_target_rate=%.17g\n"
            "homeostasis_update_interval_steps=%d\n"
            "homeostasis_record_history=%s\n"
            "homeostasis_record_interval_steps=%d\n"
            "homeostasis_record_neuron_limit=%d\n"
            "homeostasis_output_files=%s\n"
            "reward_enabled=%s\n"
            "reward_mode=%s\n"
            "reward_learning_rate=%.17g\n"
            "reward_eligibility_tau=%.17g\n"
            "reward_eligibility_min=%.17g\n"
            "reward_eligibility_max=%.17g\n"
            "reward_min=%.17g\n"
            "reward_max=%.17g\n"
            "reward_clip_enabled=%s\n"
            "reward_scheduled_event_count=%d\n"
            "reward_event_timing=queued_before_step_applied_after_eligibility\n"
            "reward_record_history=%s\n"
            "reward_record_interval_steps=%d\n"
            "reward_record_connection_limit=%d\n"
            "reward_output_files=%s\n"
            "eligibility_reset_on_reward=false\n"
            "pending_reward_consumption=one-shot\n"
            "reward_timing_reference=spike_emission\n"
            "inhibitory_reward_plasticity=disabled\n"
            "reward_homeostasis_order=reward_then_scaling\n"
            "python=NA\n"
            "python_version=NA\n"
            "pandas_version=NA\n"
            "matplotlib_version=NA\n"
            "files=config_used.ini;summary.txt;population.csv;raster.csv;neuron_%d.csv;run_manifest.txt%s\n",
            MINISNN_VERSION,
            git_commit,
            git_status,
            timestamp,
            __VERSION__,
#if defined(_WIN64)
            "x86_64",
#elif defined(_WIN32)
            "x86",
#else
            "unknown",
#endif
            source_config_path != NULL ? source_config_path : "NA",
            result->output_directory,
            config->run_name,
            result->actual_run_name,
            config->seed,
            config->neurons,
            config->topology,
            config->dt,
            config->steps,
            config->diagnostics_level,
            config->plasticity_enabled ? "true" : "false",
            config->plasticity_rule,
            config->plasticity_learning_mode,
            config->plasticity_a_plus,
            config->plasticity_a_minus,
            config->plasticity_tau_plus,
            config->plasticity_tau_minus,
            config->plasticity_trace_increment,
            config->plasticity_weight_min,
            config->plasticity_weight_max,
            config->plasticity_record_weights ? "true" : "false",
            config->plasticity_record_history ? "true" : "false",
            config->plasticity_record_interval_steps,
            config->plasticity_record_connection_limit,
            plasticity_files,
            config->homeostasis_enabled ? "true" : "false",
            config->homeostasis_intrinsic_enabled ? "true" : "false",
            config->homeostasis_synaptic_scaling_enabled ? "true" : "false",
            config->homeostasis_inhibitory_gain_enabled ? "true" : "false",
            config->homeostasis_target_rate,
            config->homeostasis_update_interval_steps,
            config->homeostasis_record_history ? "true" : "false",
            config->homeostasis_record_interval_steps,
            config->homeostasis_record_neuron_limit,
            homeostasis_files,
            config->reward_enabled ? "true" : "false",
            config->reward_mode,
            config->reward_learning_rate,
            config->reward_eligibility_tau,
            config->reward_eligibility_min,
            config->reward_eligibility_max,
            config->reward_min,
            config->reward_max,
            config->reward_clip ? "true" : "false",
            config->reward_event_count,
            config->reward_record_history ? "true" : "false",
            config->reward_record_interval_steps,
            config->reward_record_connection_limit,
            reward_files,
            config->record_neuron,
            metrics_generated ? ";metrics.csv" : "") < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int run_simulation(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    FILE *population_file,
    FILE *raster_file,
    FILE *neuron_file,
    ScenarioRunResult *result,
    PlasticityRunData *plasticity_data,
    HomeostasisRunData *homeostasis_data,
    RewardRunData *reward_data)
{
    result->spikes_total = 0;
    result->spikes_exc = 0;
    result->spikes_inh = 0;
    result->first_active_step = -1;
    result->last_active_step = -1;
    result->active_timesteps = 0;
    result->peak_activity_step = -1;
    result->peak_activity_value = 0;
    result->min_activity_value = config->neurons;

    for (int step = 0; step < config->steps; step++)
    {
        int step_spikes;
        int spikes_total = 0;
        int spikes_exc = 0;
        int spikes_inh = 0;
        double voltage_sum = 0.0;
        double syn_current_sum = 0.0;
        int record_spike = 0;
        double record_voltage = 0.0;
        double record_syn_current = 0.0;
        double record_ext_current =
            config->record_neuron < config->source_count ?
            config->input_current :
            0.0;
        double scheduled_reward = 0.0;
        int reward_component_count = 0;

        if (reward_data->enabled)
        {
            for (int i = 0; i < config->reward_event_count; i++)
            {
                if (config->reward_events[i].step == step)
                {
                    scheduled_reward += config->reward_events[i].value;
                    reward_component_count++;
                }
            }

            if (!isfinite(scheduled_reward))
                return 0;

            if (reward_component_count > 0 &&
                !minisnn_queue_reward(snn, scheduled_reward))
            {
                return 0;
            }
        }

        minisnn_clear_inputs(snn);

        for (int source = 0; source < config->source_count; source++)
        {
            if (!minisnn_set_input(snn, source, config->input_current))
                return 0;
        }

        step_spikes = minisnn_step(snn);

        if (step_spikes < 0)
            return 0;

        for (int neuron_id = 0; neuron_id < config->neurons; neuron_id++)
        {
            int spike;
            double voltage;
            double syn_current;
            int is_inh = neuron_id_is_inhibitory(
                neuron_id,
                config->neurons,
                inhibitory_count);

            if (!minisnn_get_spike(snn, neuron_id, &spike) ||
                !minisnn_get_voltage(snn, neuron_id, &voltage) ||
                !minisnn_get_synaptic_current(snn, neuron_id, &syn_current))
            {
                return 0;
            }

            voltage_sum += voltage;
            syn_current_sum += syn_current;

            if (spike)
            {
                spikes_total++;

                if (is_inh)
                    spikes_inh++;
                else
                    spikes_exc++;

                if (fprintf(
                        raster_file,
                        "%d,%d,%s\n",
                        step,
                        neuron_id,
                        type_name(
                            neuron_id,
                            config->neurons,
                            inhibitory_count)) < 0)
                {
                    return 0;
                }
            }

            if (neuron_id == config->record_neuron)
            {
                record_spike = spike;
                record_voltage = voltage;
                record_syn_current = syn_current;
            }
        }

        if (step_spikes != spikes_total)
            return 0;

        if (spikes_total > 0)
        {
            if (result->first_active_step < 0)
                result->first_active_step = step;

            result->last_active_step = step;
            result->active_timesteps++;
        }

        if (spikes_total > result->peak_activity_value)
        {
            result->peak_activity_value = spikes_total;
            result->peak_activity_step = step;
        }

        if (spikes_total < result->min_activity_value)
            result->min_activity_value = spikes_total;

        result->spikes_total += spikes_total;
        result->spikes_exc += spikes_exc;
        result->spikes_inh += spikes_inh;

        if (fprintf(
                population_file,
                "%d,%d,%d,%d,%.2f,%.2f\n",
                step,
                spikes_total,
                spikes_exc,
                spikes_inh,
                voltage_sum / (double)config->neurons,
                syn_current_sum / (double)config->neurons) < 0)
        {
            return 0;
        }

        if (fprintf(
                neuron_file,
                "%d,%.2f,%d,%.2f,%.2f\n",
                step,
                record_voltage,
                record_spike,
                record_ext_current,
                record_syn_current) < 0)
        {
            return 0;
        }

        if (plasticity_data->enabled &&
            config->plasticity_record_history &&
            (((step + 1) % config->plasticity_record_interval_steps) == 0 ||
             step + 1 == config->steps) &&
            !write_weight_history_step(snn, plasticity_data, step + 1))
        {
            return 0;
        }

        if (homeostasis_data->enabled &&
            config->homeostasis_record_history &&
            (((step + 1) % config->homeostasis_record_interval_steps) == 0 ||
             step + 1 == config->steps) &&
            !write_homeostasis_history_step(
                snn, config, homeostasis_data, step + 1))
        {
            return 0;
        }

        if (reward_data->enabled && reward_component_count > 0 &&
            (!write_reward_event_step(
                 snn, reward_data, step, reward_component_count) ||
             !write_reward_history_step(snn, reward_data, step + 1)))
        {
            return 0;
        }

        if (reward_data->enabled && config->reward_record_history &&
            (((step + 1) % config->reward_record_interval_steps) == 0 ||
             step + 1 == config->steps) &&
            !write_reward_history_step(snn, reward_data, step + 1))
        {
            return 0;
        }
    }

    return 1;
}

int scenario_runner_execute(
    const ScenarioConfig *config,
    const char *source_config_path,
    ScenarioRunResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    MiniSNNConfig minisnn_config;
    MiniSNN *snn = NULL;
    ScenarioRunResult result;
    FILE *population_file = NULL;
    FILE *raster_file = NULL;
    FILE *neuron_file = NULL;
    FILE *summary_file = NULL;
    char config_used_path[SCENARIO_OUTPUT_PATH_MAX];
    char population_path[SCENARIO_OUTPUT_PATH_MAX];
    char raster_path[SCENARIO_OUTPUT_PATH_MAX];
    char neuron_path[SCENARIO_OUTPUT_PATH_MAX];
    char summary_path[SCENARIO_OUTPUT_PATH_MAX];
    int neuron_is_inhibitory[SCENARIO_MAX_NEURONS];
    ConnectivityStats connectivity;
    PlasticityRunData plasticity_data;
    HomeostasisRunData homeostasis_data;
    RewardRunData reward_data;
    ULONGLONG wall_start;
    ULONGLONG simulation_start;
    double simulation_seconds;
    double wall_seconds;
    int metrics_generated = 0;

    if (config == NULL || out_result == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    memset(&result, 0, sizeof(result));
    memset(neuron_is_inhibitory, 0, sizeof(neuron_is_inhibitory));
    memset(&connectivity, 0, sizeof(connectivity));
    memset(&plasticity_data, 0, sizeof(plasticity_data));
    memset(&homeostasis_data, 0, sizeof(homeostasis_data));
    memset(&reward_data, 0, sizeof(reward_data));

    if (!scenario_config_validate(config, error_message, error_message_size))
        return 0;

    wall_start = GetTickCount64();

    if (!ensure_output_directory(
            config,
            result.output_directory,
            result.actual_run_name,
            sizeof(result.actual_run_name)))
    {
        set_error(error_message, error_message_size, "erro ao criar diretorio de saida");
        return 0;
    }

    if (!make_path(config_used_path, result.output_directory, "config_used.ini"))
    {
        set_error(error_message, error_message_size, "erro ao montar caminho de config_used.ini");
        return 0;
    }

    if (!write_config_used(
            config,
            source_config_path,
            config_used_path,
            error_message,
            error_message_size))
    {
        return 0;
    }

    minisnn_config.neuron_count = config->neurons;
    minisnn_config.dt = config->dt;
    minisnn_config.tau = config->tau;
    minisnn_config.v_rest = config->v_rest;
    minisnn_config.v_reset = config->v_reset;
    minisnn_config.v_threshold = config->v_threshold;
    minisnn_config.resistance = config->resistance;
    minisnn_config.synaptic_decay = config->synaptic_decay;
    minisnn_config.max_synaptic_delay = config->max_synaptic_delay;

    snn = minisnn_create_with_config(&minisnn_config);

    if (snn == NULL)
    {
        set_error(error_message, error_message_size, "erro ao criar rede MiniSNN");
        return 0;
    }

    result.inhibitory_count = calculate_inhibitory_count(config);

    if (!set_neuron_types(
            snn,
            config,
            result.inhibitory_count,
            neuron_is_inhibitory))
    {
        set_error(error_message, error_message_size, "erro ao definir tipos de neuronios");
        minisnn_destroy(&snn);
        return 0;
    }

    if (!build_topology(
            snn,
            config,
            neuron_is_inhibitory,
            &connectivity))
    {
        set_error(error_message, error_message_size, "erro ao construir topologia");
        minisnn_destroy(&snn);
        return 0;
    }

    result.connection_count = connectivity.count;
    result.self_connection_count = connectivity.self_count;
    result.excitatory_to_excitatory_count =
        connectivity.excitatory_to_excitatory_count;
    result.excitatory_to_inhibitory_count =
        connectivity.excitatory_to_inhibitory_count;
    result.inhibitory_to_excitatory_count =
        connectivity.inhibitory_to_excitatory_count;
    result.inhibitory_to_inhibitory_count =
        connectivity.inhibitory_to_inhibitory_count;
    result.topology_signature = connectivity.topology_signature;

    {
        MiniSNNPlasticityConfig plasticity_config =
            minisnn_default_plasticity_config();

        plasticity_config.enabled = config->plasticity_enabled;
        plasticity_config.rule = MINISNN_PLASTICITY_STDP_PAIR_TRACE;
        plasticity_config.learning_mode =
            strcmp(
                config->plasticity_learning_mode,
                "reward_modulated_stdp") == 0 ?
                MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP :
                MINISNN_LEARNING_MODE_DIRECT_STDP;
        plasticity_config.a_plus = config->plasticity_a_plus;
        plasticity_config.a_minus = config->plasticity_a_minus;
        plasticity_config.tau_plus = config->plasticity_tau_plus;
        plasticity_config.tau_minus = config->plasticity_tau_minus;
        plasticity_config.trace_increment = config->plasticity_trace_increment;
        plasticity_config.weight_min = config->plasticity_weight_min;
        plasticity_config.weight_max = config->plasticity_weight_max;

        if (!minisnn_set_plasticity_config(snn, &plasticity_config))
        {
            set_error(error_message, error_message_size, "erro ao configurar plasticidade");
            minisnn_destroy(&snn);
            return 0;
        }
    }

    {
        MiniSNNRewardConfig reward_config = minisnn_default_reward_config();

        reward_config.enabled = config->reward_enabled;
        reward_config.mode = MINISNN_REWARD_MODE_RSTDP;
        reward_config.learning_rate = config->reward_learning_rate;
        reward_config.eligibility_tau = config->reward_eligibility_tau;
        reward_config.eligibility_min = config->reward_eligibility_min;
        reward_config.eligibility_max = config->reward_eligibility_max;
        reward_config.reward_min = config->reward_min;
        reward_config.reward_max = config->reward_max;
        reward_config.clip_reward = config->reward_clip;

        if (!minisnn_set_reward_config(snn, &reward_config))
        {
            set_error(error_message, error_message_size, "erro ao configurar reward");
            minisnn_destroy(&snn);
            return 0;
        }
    }

    {
        MiniSNNHomeostasisConfig homeostasis_config =
            minisnn_default_homeostasis_config();

        homeostasis_config.enabled = config->homeostasis_enabled;
        homeostasis_config.intrinsic_enabled =
            config->homeostasis_intrinsic_enabled;
        homeostasis_config.target_rate = config->homeostasis_target_rate;
        homeostasis_config.rate_tau = config->homeostasis_rate_tau;
        homeostasis_config.update_interval_steps =
            (unsigned int)config->homeostasis_update_interval_steps;
        homeostasis_config.threshold_eta = config->homeostasis_threshold_eta;
        homeostasis_config.threshold_min = config->homeostasis_threshold_min;
        homeostasis_config.threshold_max = config->homeostasis_threshold_max;
        homeostasis_config.synaptic_scaling_enabled =
            config->homeostasis_synaptic_scaling_enabled;
        homeostasis_config.scaling_eta = config->homeostasis_scaling_eta;
        homeostasis_config.scaling_min_factor =
            config->homeostasis_scaling_min_factor;
        homeostasis_config.scaling_max_factor =
            config->homeostasis_scaling_max_factor;
        homeostasis_config.scaling_weight_min =
            config->homeostasis_scaling_weight_min;
        homeostasis_config.scaling_weight_max =
            config->homeostasis_scaling_weight_max;
        homeostasis_config.inhibitory_gain_enabled =
            config->homeostasis_inhibitory_gain_enabled;
        homeostasis_config.inhibitory_gain_initial =
            config->homeostasis_inhibitory_gain_initial;
        homeostasis_config.inhibitory_gain_eta =
            config->homeostasis_inhibitory_gain_eta;
        homeostasis_config.inhibitory_gain_min =
            config->homeostasis_inhibitory_gain_min;
        homeostasis_config.inhibitory_gain_max =
            config->homeostasis_inhibitory_gain_max;

        if (!minisnn_set_homeostasis_config(snn, &homeostasis_config))
        {
            set_error(error_message, error_message_size, "erro ao configurar homeostase");
            minisnn_destroy(&snn);
            return 0;
        }
    }

    if (!plasticity_run_data_prepare(
            snn,
            config,
            result.output_directory,
            &plasticity_data))
    {
        set_error(error_message, error_message_size, "erro ao preparar saidas de plasticidade");
        minisnn_destroy(&snn);
        return 0;
    }

    if (!homeostasis_run_data_prepare(
            snn,
            config,
            result.output_directory,
            &homeostasis_data))
    {
        set_error(error_message, error_message_size, "erro ao preparar saidas de homeostase");
        plasticity_run_data_close(&plasticity_data);
        minisnn_destroy(&snn);
        return 0;
    }

    if (!reward_run_data_prepare(
            snn,
            config,
            result.output_directory,
            &reward_data))
    {
        set_error(error_message, error_message_size, "erro ao preparar saidas de reward");
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    if (!open_output_files(
            config,
            result.output_directory,
            &population_file,
            &raster_file,
            &neuron_file,
            &summary_file,
            population_path,
            raster_path,
            neuron_path,
            summary_path))
    {
        set_error(error_message, error_message_size, "erro ao abrir arquivos de saida");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    simulation_start = GetTickCount64();

    if (!run_simulation(
            snn,
            config,
            result.inhibitory_count,
            population_file,
            raster_file,
            neuron_file,
            &result,
            &plasticity_data,
            &homeostasis_data,
            &reward_data))
    {
        set_error(error_message, error_message_size, "erro durante simulacao");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    simulation_seconds =
        (double)(GetTickCount64() - simulation_start) / 1000.0;

    if (!plasticity_run_data_finalize(
            snn,
            config,
            &result,
            &plasticity_data,
            simulation_seconds))
    {
        set_error(error_message, error_message_size, "erro ao finalizar saidas de plasticidade");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    if (!homeostasis_run_data_finalize(
            snn,
            config,
            &result,
            &homeostasis_data))
    {
        set_error(error_message, error_message_size, "erro ao finalizar saidas de homeostase");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    if (!reward_run_data_finalize(
            snn,
            config,
            &result,
            &reward_data))
    {
        set_error(error_message, error_message_size, "erro ao finalizar saidas de reward");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    if (!write_summary(
            summary_file,
            config,
            &result,
            &plasticity_data,
            &homeostasis_data,
            &reward_data))
    {
        set_error(error_message, error_message_size, "erro ao escrever summary.txt");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        minisnn_destroy(&snn);
        return 0;
    }

    close_file_if_open(population_file);
    close_file_if_open(raster_file);
    close_file_if_open(neuron_file);
    close_file_if_open(summary_file);
    minisnn_destroy(&snn);

    wall_seconds = (double)(GetTickCount64() - wall_start) / 1000.0;

    if (strcmp(config->diagnostics_level, "off") != 0)
    {
        if (!write_basic_metrics(
                config,
                &result,
                &connectivity,
                simulation_seconds,
                wall_seconds,
                population_path,
                raster_path,
                &plasticity_data,
                &homeostasis_data,
                &reward_data))
        {
            set_error(error_message, error_message_size, "erro ao escrever metrics.csv");
            plasticity_run_data_close(&plasticity_data);
            homeostasis_run_data_close(&homeostasis_data);
            reward_run_data_close(&reward_data);
            return 0;
        }

        metrics_generated = 1;
    }

    if (!write_run_manifest(
            config,
            &result,
            source_config_path,
            metrics_generated))
    {
        set_error(error_message, error_message_size, "erro ao escrever run_manifest.txt");
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        return 0;
    }

    if (config->history_enabled &&
        !append_scenario_history(config, &result, source_config_path, "OK"))
    {
        set_error(error_message, error_message_size, "erro ao atualizar historico de cenarios");
        plasticity_run_data_close(&plasticity_data);
        homeostasis_run_data_close(&homeostasis_data);
        reward_run_data_close(&reward_data);
        return 0;
    }

    plasticity_run_data_close(&plasticity_data);
    homeostasis_run_data_close(&homeostasis_data);
    reward_run_data_close(&reward_data);

    *out_result = result;
    return 1;
}

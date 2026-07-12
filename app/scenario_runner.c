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
#define MINISNN_VERSION "0.2"

typedef struct
{
    int count;
    int self_count;
    int excitatory_count;
    int inhibitory_count;
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
    const ScenarioRunResult *result)
{
    return fprintf(
               file,
               "run_name=%s\n"
               "actual_run_name=%s\n"
               "topology=%s\n"
               "neurons=%d\n"
               "inhibitory_count=%d\n"
               "connection_count=%d\n"
               "steps=%d\n"
               "input_current=%.6f\n"
               "source_count=%d\n"
               "spikes_total=%d\n"
               "spikes_exc=%d\n"
               "spikes_inh=%d\n"
               "first_active_step=%d\n"
               "last_active_step=%d\n",
               config->run_name,
               result->actual_run_name,
               config->topology,
               config->neurons,
               result->inhibitory_count,
               result->connection_count,
               config->steps,
               config->input_current,
               config->source_count,
               result->spikes_total,
               result->spikes_exc,
               result->spikes_inh,
               result->first_active_step,
               result->last_active_step) >= 0;
}

static int append_scenario_history(
    const ScenarioConfig *config,
    const ScenarioRunResult *result,
    const char *source_config_path,
    const char *status)
{
    int needs_header = !file_exists(SCENARIO_HISTORY_PATH);
    FILE *file = fopen(SCENARIO_HISTORY_PATH, "a");
    char timestamp[32];

    if (file == NULL)
        return 0;

    if (needs_header)
    {
        if (fprintf(
                file,
                "timestamp,run_name,actual_run_name,run_path,config_path,topology,num_neurons,steps,dt,seed,recorded_neuron,total_connections,total_spikes,first_active_step,last_active_step,status\n") < 0)
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
    const char *raster_path)
{
    char path[SCENARIO_OUTPUT_PATH_MAX];
    char timestamp[32];
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

    if (fprintf(
            file,
            "run_name,actual_run_name,run_path,timestamp,topology,network_num_neurons,run_steps,run_dt,run_simulation_duration,run_seed,run_recorded_neuron,diagnostics_level,network_total_connections,network_excitatory_neuron_count,network_inhibitory_neuron_count,activity_total_spikes,activity_mean_spikes_per_step,activity_min_spikes_per_step,activity_max_spikes_per_step,activity_active_timesteps,activity_silent_timesteps,activity_fraction,silence_fraction,activity_first_active_step,activity_last_active_step,activity_peak_step,activity_peak_value,activity_has_late_activity,neuron_mean_spikes,exc_total_spikes,inh_total_spikes,network_connection_density,network_self_connection_count,network_excitatory_connection_count,network_inhibitory_connection_count,network_weight_mean,network_weight_min,network_weight_max,network_weight_std,network_delay_mean,network_delay_min,network_delay_max,network_delay_std,network_indegree_mean,network_indegree_min,network_indegree_max,network_indegree_std,network_outdegree_mean,network_outdegree_min,network_outdegree_max,network_outdegree_std,performance_simulation_time_seconds,performance_wall_time_seconds,performance_steps_per_second,performance_neuron_updates_per_second,performance_spikes_per_second_processed,performance_population_csv_size_bytes,performance_raster_csv_size_bytes\n") < 0 ||
        fprintf(
            file,
            "%s,%s,%s,%s,%s,%d,%d,%.12g,%.12g,%u,%d,%s,%d,%d,%d,%d,%.12g,%d,%d,%d,%d,%.12g,%.12g,%d,%d,%d,%d,%s,%.12g,%d,%d,%.12g,%d,%d,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%d,%.12g,%.12g,%d,%d,%.12g,%.12g,%d,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%llu,%llu\n",
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
            file_size_bytes(raster_path)) < 0)
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
    FILE *pipe;

    if (!make_path(path, result->output_directory, "run_manifest.txt"))
        return 0;

    file = fopen(path, "w");
    if (file == NULL)
        return 0;

    current_timestamp(timestamp, sizeof(timestamp));

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
    ScenarioRunResult *result)
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
            &result))
    {
        set_error(error_message, error_message_size, "erro durante simulacao");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
        minisnn_destroy(&snn);
        return 0;
    }

    simulation_seconds =
        (double)(GetTickCount64() - simulation_start) / 1000.0;

    if (!write_summary(summary_file, config, &result))
    {
        set_error(error_message, error_message_size, "erro ao escrever summary.txt");
        close_file_if_open(population_file);
        close_file_if_open(raster_file);
        close_file_if_open(neuron_file);
        close_file_if_open(summary_file);
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
                raster_path))
        {
            set_error(error_message, error_message_size, "erro ao escrever metrics.csv");
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
        return 0;
    }

    if (config->history_enabled &&
        !append_scenario_history(config, &result, source_config_path, "OK"))
    {
        set_error(error_message, error_message_size, "erro ao atualizar historico de cenarios");
        return 0;
    }

    *out_result = result;
    return 1;
}

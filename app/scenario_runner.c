#include "scenario_runner.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minisnn.h"

#define COMMAND_BUFFER_SIZE 640

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

static int ensure_output_directory(const char *run_name, char *out_dir)
{
    if (snprintf(
            out_dir,
            SCENARIO_OUTPUT_PATH_MAX,
            "results/scenarios/%s",
            run_name) >= SCENARIO_OUTPUT_PATH_MAX)
    {
        return 0;
    }

    if (!make_directory_if_needed("results"))
        return 0;

    if (!make_directory_if_needed("results\\scenarios"))
        return 0;

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

static int neuron_is_inhibitory(int neuron_id, int neurons, int inhibitory_count)
{
    return neuron_id >= neurons - inhibitory_count;
}

static const char *type_name(
    int neuron_id,
    int neurons,
    int inhibitory_count)
{
    return neuron_is_inhibitory(neuron_id, neurons, inhibitory_count) ?
        "INH" :
        "EXC";
}

static double outgoing_weight(
    const ScenarioConfig *config,
    int source,
    int inhibitory_count)
{
    if (neuron_is_inhibitory(source, config->neurons, inhibitory_count))
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
    int inhibitory_count)
{
    for (int i = 0; i < config->neurons; i++)
    {
        MiniSNNNeuronType type =
            neuron_is_inhibitory(i, config->neurons, inhibitory_count) ?
            MINISNN_NEURON_INHIBITORY :
            MINISNN_NEURON_EXCITATORY;

        if (!minisnn_set_neuron_type(snn, i, type))
            return 0;
    }

    return 1;
}

static int connect_pair(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int source,
    int target,
    int *connection_count)
{
    double weight = outgoing_weight(config, source, inhibitory_count);

    if (!minisnn_connect_delayed(snn, source, target, weight, config->delay))
        return 0;

    (*connection_count)++;
    return 1;
}

static int build_chain(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *connection_count)
{
    for (int source = 0; source < config->neurons - 1; source++)
    {
        if (!connect_pair(
                snn,
                config,
                inhibitory_count,
                source,
                source + 1,
                connection_count))
        {
            return 0;
        }
    }

    return 1;
}

static int build_ring(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *connection_count)
{
    if (config->neurons == 1)
        return 1;

    if (!build_chain(snn, config, inhibitory_count, connection_count))
        return 0;

    return connect_pair(
        snn,
        config,
        inhibitory_count,
        config->neurons - 1,
        0,
        connection_count);
}

static int build_all_to_all(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *connection_count)
{
    for (int source = 0; source < config->neurons; source++)
    {
        for (int target = 0; target < config->neurons; target++)
        {
            if (source == target)
                continue;

            if (!connect_pair(
                    snn,
                    config,
                    inhibitory_count,
                    source,
                    target,
                    connection_count))
            {
                return 0;
            }
        }
    }

    return 1;
}

static int build_random_balanced(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *connection_count)
{
    uint32_t state = config->seed;

    if (state == 0U)
        state = 1U;

    for (int source = 0; source < config->neurons; source++)
    {
        for (int target = 0; target < config->neurons; target++)
        {
            if (source == target)
                continue;

            if (rng_next_unit(&state) < config->connection_probability)
            {
                if (!connect_pair(
                        snn,
                        config,
                        inhibitory_count,
                        source,
                        target,
                        connection_count))
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}

static int build_topology(
    MiniSNN *snn,
    const ScenarioConfig *config,
    int inhibitory_count,
    int *connection_count)
{
    *connection_count = 0;

    if (strcmp(config->topology, "chain") == 0)
        return build_chain(snn, config, inhibitory_count, connection_count);

    if (strcmp(config->topology, "ring") == 0)
        return build_ring(snn, config, inhibitory_count, connection_count);

    if (strcmp(config->topology, "all_to_all") == 0)
        return build_all_to_all(snn, config, inhibitory_count, connection_count);

    if (strcmp(config->topology, "random_balanced") == 0)
        return build_random_balanced(
            snn,
            config,
            inhibitory_count,
            connection_count);

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
            int is_inh = neuron_is_inhibitory(
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
        }

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

    if (config == NULL || out_result == NULL)
    {
        set_error(error_message, error_message_size, "argumento nulo");
        return 0;
    }

    memset(&result, 0, sizeof(result));

    if (!scenario_config_validate(config, error_message, error_message_size))
        return 0;

    if (!ensure_output_directory(config->run_name, result.output_directory))
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

    if (!set_neuron_types(snn, config, result.inhibitory_count))
    {
        set_error(error_message, error_message_size, "erro ao definir tipos de neuronios");
        minisnn_destroy(&snn);
        return 0;
    }

    if (!build_topology(
            snn,
            config,
            result.inhibitory_count,
            &result.connection_count))
    {
        set_error(error_message, error_message_size, "erro ao construir topologia");
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
        minisnn_destroy(&snn);
        return 0;
    }

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

    *out_result = result;
    return 1;
}

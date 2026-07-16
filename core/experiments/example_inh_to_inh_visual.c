#include <stdio.h>

#include "config.h"
#include "network.h"
#include "recorder.h"

#define POPULATION_SIZE 20
#define INHIBITORY_COUNT 4
#define EXPERIMENT_STEPS 450
#define TEST_W_INH -400.0
#define INPUT_SOURCE_COUNT 3
#define CASE_COUNT 2

typedef enum
{
    ARCH_ALL_TO_ALL = 0,
    ARCH_NO_INH_TO_INH = 1
} ArchitectureMode;

typedef struct
{
    const char *name;
    const char *spikes_filename;
    const char *metrics_filename;
    ArchitectureMode mode;
} VisualCase;

typedef struct
{
    int total_spikes;
    int first_active_step;
    int last_active_step;
    int first_full_sync_step;
    int source_spiked[INPUT_SOURCE_COUNT];
} VisualResult;

static int is_inhibitory_neuron(int neuron_id)
{
    return neuron_id >= POPULATION_SIZE - INHIBITORY_COUNT;
}

static void reset_result(VisualResult *result)
{
    result->total_spikes = 0;
    result->first_active_step = -1;
    result->last_active_step = -1;
    result->first_full_sync_step = -1;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        result->source_spiked[i] = 0;
}

static void close_resources(
    Network *net,
    PopulationRecorder *population_recorder,
    FILE *spike_file,
    int net_initialized)
{
    population_recorder_close(population_recorder);

    if (spike_file != NULL)
        fclose(spike_file);

    if (net_initialized)
        network_destroy(net);
}

static int configure_neuron_types(Network *net)
{
    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        NeuronType type = NEURON_EXCITATORY;

        if (is_inhibitory_neuron(neuron_id))
            type = NEURON_INHIBITORY;

        if (!network_set_neuron_type(net, neuron_id, type))
            return 0;
    }

    return 1;
}

static int should_connect(const VisualCase *test_case, int source, int target)
{
    if (source == target)
        return 0;

    if (test_case->mode == ARCH_NO_INH_TO_INH &&
        is_inhibitory_neuron(source) &&
        is_inhibitory_neuron(target))
    {
        return 0;
    }

    return 1;
}

static int connect_network(Network *net, const VisualCase *test_case)
{
    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (is_inhibitory_neuron(source))
            weight = TEST_W_INH;

        for (int target = 0; target < POPULATION_SIZE; target++)
        {
            if (!should_connect(test_case, source, target))
                continue;

            if (!network_connect_delayed(net, source, target, weight, 1))
                return 0;
        }
    }

    return 1;
}

static int apply_input_currents(Network *net)
{
    network_clear_external_currents(net);

    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (!network_set_external_current(net, neuron_id, I_EXT))
            return 0;
    }

    return 1;
}

static const char *neuron_type_name(NeuronType type)
{
    return type == NEURON_INHIBITORY ? "INH" : "EXC";
}

static int record_spike_raster(FILE *spike_file, const Network *net, int step)
{
    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (!net->spikes[neuron_id])
            continue;

        if (fprintf(
                spike_file,
                "%d,%d,%s\n",
                step,
                neuron_id,
                neuron_type_name(net->neurons[neuron_id].type)) < 0)
        {
            return 0;
        }
    }

    return 1;
}

static void update_result(
    const Network *net,
    int step,
    int step_spikes,
    VisualResult *result)
{
    result->total_spikes += step_spikes;

    if (step_spikes > 0)
    {
        if (result->first_active_step < 0)
            result->first_active_step = step;

        result->last_active_step = step;
    }

    if (result->first_full_sync_step < 0 &&
        step_spikes == POPULATION_SIZE)
    {
        result->first_full_sync_step = step;
    }

    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (net->spikes[neuron_id])
            result->source_spiked[neuron_id] = 1;
    }
}

static int validate_sources(const VisualCase *test_case, const VisualResult *result)
{
    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (!result->source_spiked[neuron_id])
        {
            printf(
                "Erro: fonte %d nunca disparou no caso %s.\n",
                neuron_id,
                test_case->name);
            return 0;
        }
    }

    return 1;
}

static int run_visual_case(const VisualCase *test_case, VisualResult *result)
{
    Network net;
    PopulationRecorder population_recorder = {NULL};
    FILE *spike_file = NULL;
    int net_initialized = 0;

    reset_result(result);

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf("Erro: falha ao criar rede no caso %s.\n", test_case->name);
        return 0;
    }

    net_initialized = 1;

    if (!configure_neuron_types(&net))
    {
        printf("Erro: falha ao configurar tipos no caso %s.\n",
               test_case->name);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!connect_network(&net, test_case))
    {
        printf("Erro: falha ao criar conexoes no caso %s.\n", test_case->name);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    spike_file = fopen(test_case->spikes_filename, "w");

    if (spike_file == NULL)
    {
        printf("Erro: nao foi possivel criar %s.\n",
               test_case->spikes_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (fprintf(spike_file, "tempo,neuronio,tipo\n") < 0)
    {
        printf("Erro: falha ao escrever cabecalho de %s.\n",
               test_case->spikes_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_open(
            &population_recorder,
            test_case->metrics_filename))
    {
        printf("Erro: nao foi possivel criar %s.\n",
               test_case->metrics_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf("Erro: falha ao escrever cabecalho de %s.\n",
               test_case->metrics_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf("Erro: falha ao aplicar estimulo no caso %s.\n",
                   test_case->name);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        step_spikes = network_update(&net);

        if (step_spikes < 0)
        {
            printf("Erro: atividade invalida no caso %s.\n", test_case->name);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        update_result(&net, step, step_spikes, result);

        if (!record_spike_raster(spike_file, &net, step))
        {
            printf("Erro: falha ao registrar raster no caso %s.\n",
                   test_case->name);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        if (!population_recorder_record(&population_recorder, &net, step))
        {
            printf("Erro: falha ao registrar metricas no caso %s.\n",
                   test_case->name);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }
    }

    if (!validate_sources(test_case, result))
    {
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    close_resources(&net, &population_recorder, spike_file, net_initialized);
    return 1;
}

static void print_result(const VisualCase *test_case, const VisualResult *result)
{
    printf("%s:\n", test_case->name);
    printf("  spikes totais: %d\n", result->total_spikes);
    printf("  primeiro timestep ativo: %d\n", result->first_active_step);
    printf("  ultimo timestep ativo: %d\n", result->last_active_step);
    printf("  primeiro timestep com todos os 20 neuronios: %d\n",
           result->first_full_sync_step);
}

int main(void)
{
    VisualCase cases[CASE_COUNT] =
    {
        {
            "all_to_all",
            "results/experiments/inh_to_inh/all_to_all_-400_spikes.csv",
            "results/experiments/inh_to_inh/all_to_all_-400_metrics.csv",
            ARCH_ALL_TO_ALL
        },
        {
            "no_inh_to_inh",
            "results/experiments/inh_to_inh/no_inh_to_inh_-400_spikes.csv",
            "results/experiments/inh_to_inh/no_inh_to_inh_-400_metrics.csv",
            ARCH_NO_INH_TO_INH
        }
    };
    VisualResult results[CASE_COUNT];

    for (int i = 0; i < CASE_COUNT; i++)
    {
        if (!run_visual_case(&cases[i], &results[i]))
            return 1;
    }

    printf("=== Visualizacao INH -> INH, peso -400 ===\n\n");

    for (int i = 0; i < CASE_COUNT; i++)
    {
        print_result(&cases[i], &results[i]);
        printf("\n");
    }

    return 0;
}

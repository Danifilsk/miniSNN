#include <stdio.h>

#include "config.h"
#include "network.h"
#include "recorder.h"

#define POPULATION_SIZE 20
#define INHIBITORY_COUNT 4
#define EXPERIMENT_STEPS 450
#define INPUT_SOURCE_COUNT 3
#define DETAIL_START_STEP 270
#define DETAIL_END_STEP 350

typedef struct
{
    double inhibitory_weight;
    const char *spikes_filename;
    const char *metrics_filename;
} RegimeConfig;

typedef struct
{
    int total_spikes;
    int first_active_step;
    int last_active_step;
    int peak_spikes;
    int source_spiked[INPUT_SOURCE_COUNT];
} RegimeResult;

static void reset_result(RegimeResult *result)
{
    result->total_spikes = 0;
    result->first_active_step = -1;
    result->last_active_step = -1;
    result->peak_spikes = 0;

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
    for (int i = 0; i < POPULATION_SIZE; i++)
    {
        NeuronType type = NEURON_EXCITATORY;

        if (i >= POPULATION_SIZE - INHIBITORY_COUNT)
            type = NEURON_INHIBITORY;

        if (!network_set_neuron_type(net, i, type))
            return 0;
    }

    return 1;
}

static int connect_all_to_all(Network *net, double inhibitory_weight)
{
    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (source >= POPULATION_SIZE - INHIBITORY_COUNT)
            weight = inhibitory_weight;

        for (int target = 0; target < POPULATION_SIZE; target++)
        {
            if (target == source)
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

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!network_set_external_current(net, i, I_EXT))
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
        if (net->spikes[neuron_id])
        {
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
    }

    return 1;
}

static void print_detail_line(const Network *net, int step)
{
    printf("t=%d | EXC:", step);

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (net->spikes[neuron_id] &&
            net->neurons[neuron_id].type == NEURON_EXCITATORY)
        {
            printf(" %d", neuron_id);
        }
    }

    printf(" | INH:");

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (net->spikes[neuron_id] &&
            net->neurons[neuron_id].type == NEURON_INHIBITORY)
        {
            printf(" %d", neuron_id);
        }
    }

    printf("\n");
}

static void update_result(
    const Network *net,
    int step,
    int step_spikes,
    RegimeResult *result)
{
    result->total_spikes += step_spikes;

    if (step_spikes > result->peak_spikes)
        result->peak_spikes = step_spikes;

    if (step_spikes > 0)
    {
        if (result->first_active_step < 0)
            result->first_active_step = step;

        result->last_active_step = step;
    }

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (net->spikes[i])
            result->source_spiked[i] = 1;
    }
}

static int validate_sources(
    const RegimeConfig *config,
    const RegimeResult *result)
{
    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
    {
        if (!result->source_spiked[i])
        {
            printf(
                "Erro: fonte %d nunca disparou para peso %.2f.\n",
                i,
                config->inhibitory_weight);
            return 0;
        }
    }

    return 1;
}

static int run_regime(
    const RegimeConfig *config,
    RegimeResult *result)
{
    Network net;
    PopulationRecorder population_recorder = {NULL};
    FILE *spike_file = NULL;
    int net_initialized = 0;

    reset_result(result);

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf(
            "Erro: falha ao criar rede para peso %.2f.\n",
            config->inhibitory_weight);
        return 0;
    }

    net_initialized = 1;

    if (!configure_neuron_types(&net))
    {
        printf(
            "Erro: falha ao configurar tipos para peso %.2f.\n",
            config->inhibitory_weight);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!connect_all_to_all(&net, config->inhibitory_weight))
    {
        printf(
            "Erro: falha ao criar conexoes para peso %.2f.\n",
            config->inhibitory_weight);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    spike_file = fopen(config->spikes_filename, "w");

    if (spike_file == NULL)
    {
        printf(
            "Erro: nao foi possivel criar %s.\n",
            config->spikes_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (fprintf(spike_file, "tempo,neuronio,tipo\n") < 0)
    {
        printf(
            "Erro: falha ao escrever cabecalho de %s.\n",
            config->spikes_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_open(
            &population_recorder,
            config->metrics_filename))
    {
        printf(
            "Erro: nao foi possivel criar %s.\n",
            config->metrics_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf(
            "Erro: falha ao escrever cabecalho de %s.\n",
            config->metrics_filename);
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    printf("=== Regime peso %.2f ===\n", config->inhibitory_weight);

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf(
                "Erro: falha ao aplicar estimulo para peso %.2f.\n",
                config->inhibitory_weight);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        step_spikes = network_update(&net);
        update_result(&net, step, step_spikes, result);

        if (!record_spike_raster(spike_file, &net, step))
        {
            printf(
                "Erro: falha ao registrar raster para peso %.2f.\n",
                config->inhibitory_weight);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        if (!population_recorder_record(&population_recorder, &net, step))
        {
            printf(
                "Erro: falha ao registrar metricas para peso %.2f.\n",
                config->inhibitory_weight);
            close_resources(
                &net,
                &population_recorder,
                spike_file,
                net_initialized);
            return 0;
        }

        if (step >= DETAIL_START_STEP &&
            step <= DETAIL_END_STEP &&
            step_spikes > 0)
        {
            print_detail_line(&net, step);
        }
    }

    if (!validate_sources(config, result))
    {
        close_resources(&net, &population_recorder, spike_file, net_initialized);
        return 0;
    }

    printf("spikes totais: %d\n", result->total_spikes);
    printf("primeiro timestep ativo: %d\n", result->first_active_step);
    printf("ultimo timestep ativo: %d\n", result->last_active_step);
    printf("pico de spikes: %d\n\n", result->peak_spikes);

    close_resources(&net, &population_recorder, spike_file, net_initialized);
    return 1;
}

int main(void)
{
    RegimeConfig configs[] =
    {
        {
            -355.0,
            "results/experiments/inhibition_regimes/regime_-355_spikes.csv",
            "results/experiments/inhibition_regimes/regime_-355_metrics.csv"
        },
        {
            -375.0,
            "results/experiments/inhibition_regimes/regime_-375_spikes.csv",
            "results/experiments/inhibition_regimes/regime_-375_metrics.csv"
        },
        {
            -400.0,
            "results/experiments/inhibition_regimes/regime_-400_spikes.csv",
            "results/experiments/inhibition_regimes/regime_-400_metrics.csv"
        },
        {
            -410.0,
            "results/experiments/inhibition_regimes/regime_-410_spikes.csv",
            "results/experiments/inhibition_regimes/regime_-410_metrics.csv"
        }
    };
    int config_count = (int)(sizeof(configs) / sizeof(configs[0]));
    RegimeResult results[sizeof(configs) / sizeof(configs[0])];

    for (int i = 0; i < config_count; i++)
    {
        if (!run_regime(&configs[i], &results[i]))
            return 1;
    }

    return 0;
}

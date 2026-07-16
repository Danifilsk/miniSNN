#include <stdio.h>

#include "config.h"
#include "network.h"

#define POPULATION_SIZE 20
#define INHIBITORY_COUNT 4
#define EXPERIMENT_STEPS 1000
#define INPUT_SOURCE_COUNT 3
#define ARCHITECTURE_COUNT 2

typedef enum
{
    ARCH_ALL_TO_ALL = 0,
    ARCH_NO_INH_TO_INH = 1
} ArchitectureMode;

typedef struct
{
    const char *name;
    ArchitectureMode mode;
} ArchitectureConfig;

typedef struct
{
    const char *architecture;
    double inhibitory_weight;
    int total_spikes;
    int peak_spikes;
    int active_timesteps;
    int last_active_step;
    int active_neurons;
    int first_full_sync_step;
    int first_exc_without_all_inh_step;
    int source_spiked[INPUT_SOURCE_COUNT];
    int neuron_spiked[POPULATION_SIZE];
    int population_recruited;
} ComparisonResult;

static int is_inhibitory_neuron(int neuron_id)
{
    return neuron_id >= POPULATION_SIZE - INHIBITORY_COUNT;
}

static int is_input_source(int neuron_id)
{
    return neuron_id >= 0 && neuron_id < INPUT_SOURCE_COUNT;
}

static void reset_result(
    ComparisonResult *result,
    const char *architecture,
    double inhibitory_weight)
{
    result->architecture = architecture;
    result->inhibitory_weight = inhibitory_weight;
    result->total_spikes = 0;
    result->peak_spikes = 0;
    result->active_timesteps = 0;
    result->last_active_step = -1;
    result->active_neurons = 0;
    result->first_full_sync_step = -1;
    result->first_exc_without_all_inh_step = -1;
    result->population_recruited = 0;

    for (int i = 0; i < INPUT_SOURCE_COUNT; i++)
        result->source_spiked[i] = 0;

    for (int i = 0; i < POPULATION_SIZE; i++)
        result->neuron_spiked[i] = 0;
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

static int should_connect(
    const ArchitectureConfig *architecture,
    int source,
    int target)
{
    if (source == target)
        return 0;

    if (architecture->mode == ARCH_NO_INH_TO_INH &&
        is_inhibitory_neuron(source) &&
        is_inhibitory_neuron(target))
    {
        return 0;
    }

    return 1;
}

static int connect_network(
    Network *net,
    const ArchitectureConfig *architecture,
    double inhibitory_weight)
{
    for (int source = 0; source < POPULATION_SIZE; source++)
    {
        double weight = W_EXC;

        if (is_inhibitory_neuron(source))
            weight = inhibitory_weight;

        for (int target = 0; target < POPULATION_SIZE; target++)
        {
            if (!should_connect(architecture, source, target))
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

static void update_metrics(
    const Network *net,
    int step,
    int step_spikes,
    ComparisonResult *result)
{
    int exc_spikes = 0;
    int inh_spikes = 0;
    int recruited_spikes = 0;
    int recurrent_exc_spikes = 0;
    int was_recruited = result->population_recruited;

    result->total_spikes += step_spikes;

    if (step_spikes > result->peak_spikes)
        result->peak_spikes = step_spikes;

    if (step_spikes > 0)
    {
        result->active_timesteps++;
        result->last_active_step = step;
    }

    if (result->first_full_sync_step < 0 &&
        step_spikes == POPULATION_SIZE)
    {
        result->first_full_sync_step = step;
    }

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (!net->spikes[neuron_id])
            continue;

        result->neuron_spiked[neuron_id] = 1;

        if (is_input_source(neuron_id))
            result->source_spiked[neuron_id] = 1;
        else
            recruited_spikes++;

        if (is_inhibitory_neuron(neuron_id))
        {
            inh_spikes++;
        }
        else
        {
            exc_spikes++;

            if (!is_input_source(neuron_id))
                recurrent_exc_spikes++;
        }
    }

    if (!result->population_recruited && recruited_spikes > 0)
        result->population_recruited = 1;

    if (was_recruited &&
        result->first_exc_without_all_inh_step < 0 &&
        recurrent_exc_spikes > 0 &&
        exc_spikes > 0 &&
        inh_spikes < INHIBITORY_COUNT)
    {
        result->first_exc_without_all_inh_step = step;
    }
}

static int finalize_result(ComparisonResult *result)
{
    for (int neuron_id = 0; neuron_id < INPUT_SOURCE_COUNT; neuron_id++)
    {
        if (!result->source_spiked[neuron_id])
        {
            printf(
                "Erro: fonte %d nunca disparou em %s com peso %.2f.\n",
                neuron_id,
                result->architecture,
                result->inhibitory_weight);
            return 0;
        }
    }

    for (int neuron_id = 0; neuron_id < POPULATION_SIZE; neuron_id++)
    {
        if (result->neuron_spiked[neuron_id])
            result->active_neurons++;
    }

    return 1;
}

static int run_case(
    const ArchitectureConfig *architecture,
    double inhibitory_weight,
    ComparisonResult *result)
{
    Network net;

    reset_result(result, architecture->name, inhibitory_weight);

    if (!network_init(&net, POPULATION_SIZE))
    {
        printf(
            "Erro: falha ao criar rede em %s com peso %.2f.\n",
            architecture->name,
            inhibitory_weight);
        return 0;
    }

    if (!configure_neuron_types(&net))
    {
        printf(
            "Erro: falha ao configurar tipos em %s com peso %.2f.\n",
            architecture->name,
            inhibitory_weight);
        network_destroy(&net);
        return 0;
    }

    if (!connect_network(&net, architecture, inhibitory_weight))
    {
        printf(
            "Erro: falha ao criar conexoes em %s com peso %.2f.\n",
            architecture->name,
            inhibitory_weight);
        network_destroy(&net);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        int step_spikes;

        if (!apply_input_currents(&net))
        {
            printf(
                "Erro: falha ao aplicar estimulo em %s com peso %.2f.\n",
                architecture->name,
                inhibitory_weight);
            network_destroy(&net);
            return 0;
        }

        step_spikes = network_update(&net);

        if (step_spikes < 0)
        {
            printf(
                "Erro: atividade invalida em %s com peso %.2f.\n",
                architecture->name,
                inhibitory_weight);
            network_destroy(&net);
            return 0;
        }

        update_metrics(&net, step, step_spikes, result);
    }

    network_destroy(&net);
    return finalize_result(result);
}

static int write_csv_row(FILE *csv, const ComparisonResult *result)
{
    if (fprintf(
            csv,
            "%s,%.2f,%d,%d,%d,%d,%d,%d,%d\n",
            result->architecture,
            result->inhibitory_weight,
            result->total_spikes,
            result->peak_spikes,
            result->active_timesteps,
            result->last_active_step,
            result->active_neurons,
            result->first_full_sync_step,
            result->first_exc_without_all_inh_step) < 0)
    {
        return 0;
    }

    return 1;
}

static void print_case_result(const ComparisonResult *result)
{
    printf("%s:\n", result->architecture);
    printf("  spikes totais: %d\n", result->total_spikes);
    printf("  pico: %d\n", result->peak_spikes);
    printf("  ultimo passo ativo: %d\n", result->last_active_step);
    printf("  sincronizacao total: %d\n", result->first_full_sync_step);
    printf(
        "  EXC ativo sem INH: %d\n",
        result->first_exc_without_all_inh_step);
}

static void print_architecture_summary(
    const char *title,
    ComparisonResult results[][ARCHITECTURE_COUNT],
    int weight_count,
    int architecture_index)
{
    int total_spikes = 0;
    int sync_cases = 0;
    int exc_without_inh_cases = 0;
    int last_active_max = -1;

    for (int i = 0; i < weight_count; i++)
    {
        const ComparisonResult *result = &results[i][architecture_index];

        total_spikes += result->total_spikes;

        if (result->first_full_sync_step >= 0)
            sync_cases++;

        if (result->first_exc_without_all_inh_step >= 0)
            exc_without_inh_cases++;

        if (result->last_active_step > last_active_max)
            last_active_max = result->last_active_step;
    }

    printf("%s:\n", title);
    printf("  spikes totais somados: %d\n", total_spikes);
    printf("  casos com sincronizacao total: %d de %d\n",
           sync_cases,
           weight_count);
    printf("  casos com EXC ativo sem INH completo: %d de %d\n",
           exc_without_inh_cases,
           weight_count);
    printf("  maior ultimo passo ativo: %d\n", last_active_max);
}

int main(void)
{
    ArchitectureConfig architectures[ARCHITECTURE_COUNT] =
    {
        {"all_to_all", ARCH_ALL_TO_ALL},
        {"no_inh_to_inh", ARCH_NO_INH_TO_INH}
    };
    double inhibitory_weights[] =
    {
        -375.0,
        -400.0,
        -425.0
    };
    int weight_count =
        (int)(sizeof(inhibitory_weights) / sizeof(inhibitory_weights[0]));
    ComparisonResult results[
        sizeof(inhibitory_weights) / sizeof(inhibitory_weights[0])]
        [ARCHITECTURE_COUNT];
    FILE *csv = fopen("results/experiments/inh_to_inh/inh_to_inh_comparison.csv", "w");

    if (csv == NULL)
    {
        printf("Erro: nao foi possivel criar results/experiments/inh_to_inh/inh_to_inh_comparison.csv.\n");
        return 1;
    }

    if (fprintf(
            csv,
            "arquitetura,peso_inibitorio,spikes_totais,pico_spikes,"
            "timesteps_ativos,ultimo_timestep_ativo,neuronios_ativos,"
            "primeiro_timestep_sincronizacao_total,"
            "primeiro_timestep_exc_sem_inh\n") < 0)
    {
        printf("Erro: nao foi possivel escrever cabecalho do CSV.\n");
        fclose(csv);
        return 1;
    }

    printf("=== Comparacao INH -> INH ===\n");

    for (int i = 0; i < weight_count; i++)
    {
        printf("\nPeso inibitorio: %.2f\n\n", inhibitory_weights[i]);

        for (int j = 0; j < ARCHITECTURE_COUNT; j++)
        {
            if (!run_case(
                    &architectures[j],
                    inhibitory_weights[i],
                    &results[i][j]))
            {
                fclose(csv);
                return 1;
            }

            if (!write_csv_row(csv, &results[i][j]))
            {
                printf("Erro: nao foi possivel escrever linha do CSV.\n");
                fclose(csv);
                return 1;
            }

            print_case_result(&results[i][j]);
            printf("\n");
        }
    }

    fclose(csv);

    printf("Com INH->INH:\n");
    print_architecture_summary(
        "all_to_all",
        results,
        weight_count,
        ARCH_ALL_TO_ALL);
    printf("\n");
    printf("Sem INH->INH:\n");
    print_architecture_summary(
        "no_inh_to_inh",
        results,
        weight_count,
        ARCH_NO_INH_TO_INH);

    return 0;
}

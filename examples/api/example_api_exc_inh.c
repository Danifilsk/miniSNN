#include <stdio.h>
#include <stdlib.h>

#include "minisnn.h"

#define NEURON_COUNT 3
#define STEPS 400
#define TARGET_NEURON 2
#define EXC_ONLY_CSV_PATH "results/api/api_exc_only_target.csv"
#define EXC_INH_CSV_PATH "results/api/api_exc_inh_target.csv"

static int ensure_results_directory(void)
{
    if (system("if not exist results mkdir results") != 0)
        return 0;

    if (system("if not exist results\\api mkdir results\\api") != 0)
        return 0;

    return 1;
}

typedef struct
{
    const char *name;
    const char *filename;
    int use_inhibition;
    double min_target_voltage;
    double max_target_voltage;
    double first_target_current;
    int has_target_current;
    int source0_spiked;
    int source1_spiked;
} ScenarioResult;

static void close_resources(MiniSNN **brain, FILE *csv)
{
    if (csv != NULL)
        fclose(csv);

    minisnn_destroy(brain);
}

static int configure_common_types(MiniSNN *brain)
{
    return minisnn_set_neuron_type(brain, 0, MINISNN_NEURON_EXCITATORY) &&
           minisnn_set_neuron_type(brain, 1, MINISNN_NEURON_INHIBITORY) &&
           minisnn_set_neuron_type(brain, 2, MINISNN_NEURON_EXCITATORY);
}

static int run_scenario(ScenarioResult *result)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *brain;
    FILE *csv = NULL;
    int has_voltage = 0;

    config.neuron_count = NEURON_COUNT;

    brain = minisnn_create_with_config(&config);

    if (brain == NULL)
    {
        printf("Erro ao criar rede %s.\n", result->name);
        return 0;
    }

    if (!configure_common_types(brain))
    {
        printf("Erro ao configurar tipos em %s.\n", result->name);
        close_resources(&brain, csv);
        return 0;
    }

    if (!minisnn_connect_delayed(brain, 0, TARGET_NEURON, 200.0, 1))
    {
        printf("Erro ao criar conexao excitatoria em %s.\n", result->name);
        close_resources(&brain, csv);
        return 0;
    }

    if (result->use_inhibition &&
        !minisnn_connect_delayed(brain, 1, TARGET_NEURON, -250.0, 1))
    {
        printf("Erro ao criar conexao inhibitoria em %s.\n", result->name);
        close_resources(&brain, csv);
        return 0;
    }

    if (!minisnn_set_input(brain, 0, 20.0))
    {
        printf("Erro ao aplicar entrada excitatoria em %s.\n", result->name);
        close_resources(&brain, csv);
        return 0;
    }

    if (result->use_inhibition &&
        !minisnn_set_input(brain, 1, 20.0))
    {
        printf("Erro ao aplicar entrada inhibitoria em %s.\n", result->name);
        close_resources(&brain, csv);
        return 0;
    }

    csv = fopen(result->filename, "w");

    if (csv == NULL)
    {
        printf("Erro ao criar %s.\n", result->filename);
        close_resources(&brain, csv);
        return 0;
    }

    if (fprintf(csv, "tempo,voltagem,spike,corrente_sinaptica\n") < 0)
    {
        printf("Erro ao escrever cabecalho em %s.\n", result->filename);
        close_resources(&brain, csv);
        return 0;
    }

    result->has_target_current = 0;
    result->source0_spiked = 0;
    result->source1_spiked = 0;

    for (int step = 0; step < STEPS; step++)
    {
        int source0_spike;
        int source1_spike;
        int target_spike;
        double target_voltage;
        double target_current;

        if (minisnn_step(brain) < 0)
        {
            printf("Erro ao executar timestep em %s.\n", result->name);
            close_resources(&brain, csv);
            return 0;
        }

        if (!minisnn_get_spike(brain, 0, &source0_spike) ||
            !minisnn_get_spike(brain, 1, &source1_spike) ||
            !minisnn_get_spike(brain, TARGET_NEURON, &target_spike) ||
            !minisnn_get_voltage(brain, TARGET_NEURON, &target_voltage) ||
            !minisnn_get_synaptic_current(
                brain,
                TARGET_NEURON,
                &target_current))
        {
            printf("Erro ao consultar estado em %s.\n", result->name);
            close_resources(&brain, csv);
            return 0;
        }

        if (source0_spike)
            result->source0_spiked = 1;

        if (source1_spike)
            result->source1_spiked = 1;

        if (!has_voltage)
        {
            result->min_target_voltage = target_voltage;
            result->max_target_voltage = target_voltage;
            has_voltage = 1;
        }
        else
        {
            if (target_voltage < result->min_target_voltage)
                result->min_target_voltage = target_voltage;

            if (target_voltage > result->max_target_voltage)
                result->max_target_voltage = target_voltage;
        }

        if (!result->has_target_current &&
            (target_current < -1e-12 || target_current > 1e-12))
        {
            result->first_target_current = target_current;
            result->has_target_current = 1;
        }

        if (fprintf(
                csv,
                "%d,%.2f,%d,%.2f\n",
                step,
                target_voltage,
                target_spike,
                target_current) < 0)
        {
            printf("Erro ao escrever CSV em %s.\n", result->name);
            close_resources(&brain, csv);
            return 0;
        }
    }

    close_resources(&brain, csv);

    if (!result->source0_spiked)
    {
        printf("Erro: fonte 0 nunca disparou em %s.\n", result->name);
        return 0;
    }

    if (result->use_inhibition && !result->source1_spiked)
    {
        printf("Erro: fonte 1 nunca disparou em %s.\n", result->name);
        return 0;
    }

    if (!result->has_target_current)
        result->first_target_current = 0.0;

    return 1;
}

int main(void)
{
    ScenarioResult exc_only =
    {
        "EXC-only",
        EXC_ONLY_CSV_PATH,
        0,
        0.0,
        0.0,
        0.0,
        0,
        0,
        0
    };
    ScenarioResult exc_inh =
    {
        "EXC/INH",
        EXC_INH_CSV_PATH,
        1,
        0.0,
        0.0,
        0.0,
        0,
        0,
        0
    };

    if (!ensure_results_directory())
    {
        printf("Erro ao criar diretorio results/api.\n");
        return 1;
    }

    if (!run_scenario(&exc_only))
        return 1;

    if (!run_scenario(&exc_inh))
        return 1;

    printf("=== API: EXC versus EXC/INH ===\n\n");
    printf("EXC-only:\n");
    printf("  menor potencial do alvo: %.2f\n", exc_only.min_target_voltage);
    printf("  maior potencial do alvo: %.2f\n", exc_only.max_target_voltage);
    printf(
        "  primeira corrente sinaptica do alvo: %.2f\n",
        exc_only.first_target_current);
    printf("\n");
    printf("EXC/INH:\n");
    printf("  menor potencial do alvo: %.2f\n", exc_inh.min_target_voltage);
    printf("  maior potencial do alvo: %.2f\n", exc_inh.max_target_voltage);
    printf(
        "  primeira corrente sinaptica do alvo: %.2f\n",
        exc_inh.first_target_current);
    printf("\n");
    printf("arquivos gerados:\n");
    printf("- %s\n", EXC_ONLY_CSV_PATH);
    printf("- %s\n", EXC_INH_CSV_PATH);

    return 0;
}

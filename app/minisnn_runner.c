#include <stdio.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

static void print_usage(const char *program_name)
{
    printf("Uso: %s <arquivo.ini>\n", program_name);
    printf("\nTopologias suportadas:\n");
    printf("- chain\n");
    printf("- ring\n");
    printf("- all_to_all\n");
    printf("- random\n");
    printf("- random_balanced\n");
    printf("- small_world\n");
    printf("- feedforward\n");
}

static void print_summary(
    const ScenarioConfig *config,
    const ScenarioRunResult *result)
{
    printf("=== miniSNN scenario ===\n");
    printf("run_name: %s\n", config->run_name);
    printf("actual_run_name: %s\n", result->actual_run_name);
    printf("topology: %s\n", config->topology);
    printf("neurons: %d\n", config->neurons);
    printf("inhibitory_count: %d\n", result->inhibitory_count);
    printf("connection_count: %d\n", result->connection_count);
    printf("steps: %d\n", config->steps);
    printf("input_current: %.2f\n", config->input_current);
    printf("source_count: %d\n", config->source_count);
    printf("spikes_total: %d\n", result->spikes_total);
    printf("spikes_exc: %d\n", result->spikes_exc);
    printf("spikes_inh: %d\n", result->spikes_inh);
    printf("first_active_step: %d\n", result->first_active_step);
    printf("last_active_step: %d\n", result->last_active_step);
    printf("diagnostics_level: %s\n", config->diagnostics_level);
}

int main(int argc, char **argv)
{
    ScenarioConfig scenario;
    ScenarioRunResult result;
    char error_message[256];

    if (argc != 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    if (!scenario_config_load_file(
            argv[1],
            &scenario,
            error_message,
            sizeof(error_message)))
    {
        printf("Erro ao carregar cenario: %s\n", error_message);
        return 1;
    }

    if (scenario.auto_unique_run)
    {
        printf("Aviso: auto_unique_run ativo; se '%s' ja existir, uma pasta unica sera criada.\n",
               scenario.run_name);
    }
    else
    {
        printf("Aviso: executar novamente o run_name '%s' sobrescreve resultados anteriores.\n",
               scenario.run_name);
    }

    if (!scenario_runner_execute(
            &scenario,
            argv[1],
            &result,
            error_message,
            sizeof(error_message)))
    {
        printf("Erro ao executar cenario: %s\n", error_message);
        return 1;
    }

    print_summary(&scenario, &result);
    printf("\nArquivos criados:\n");
    printf("- %s/config_used.ini\n", result.output_directory);
    printf("- %s/summary.txt\n", result.output_directory);
    printf("- %s/population.csv\n", result.output_directory);
    printf("- %s/raster.csv\n", result.output_directory);
    printf("- %s/neuron_%d.csv\n",
           result.output_directory,
           scenario.record_neuron);
    printf("- %s/run_manifest.txt\n", result.output_directory);

    if (strcmp(scenario.diagnostics_level, "off") != 0)
        printf("- %s/metrics.csv\n", result.output_directory);

    return 0;
}

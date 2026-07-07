#include <stdio.h>

#include "config.h"
#include "network.h"
#include "recorder.h"

#define EXPERIMENT_STEPS 400
#define SOURCE_EXC 0
#define SOURCE_INH 1
#define TARGET 2

typedef struct
{
    int first_exc_spike_step;
    int first_inh_spike_step;
    int first_target_current_step;
    double first_target_current;
    double max_target_potential;
    double min_target_potential;
} ExperimentResult;

static int is_nonzero(double value)
{
    return value < -1e-9 || value > 1e-9;
}

static void reset_result(ExperimentResult *result)
{
    result->first_exc_spike_step = -1;
    result->first_inh_spike_step = -1;
    result->first_target_current_step = -1;
    result->first_target_current = 0.0;
    result->max_target_potential = V_REST;
    result->min_target_potential = V_REST;
}

static void close_resources(
    Network *net,
    NeuronRecorder *neuron_recorder,
    PopulationRecorder *population_recorder,
    int net_initialized)
{
    population_recorder_close(population_recorder);
    neuron_recorder_close(neuron_recorder);

    if (net_initialized)
        network_destroy(net);
}

static int configure_common_network(Network *net)
{
    if (!network_set_neuron_type(net, SOURCE_EXC, NEURON_EXCITATORY))
        return 0;

    if (!network_set_neuron_type(net, SOURCE_INH, NEURON_INHIBITORY))
        return 0;

    if (!network_set_neuron_type(net, TARGET, NEURON_EXCITATORY))
        return 0;

    return 1;
}

static int run_scenario(
    const char *label,
    int balanced,
    const char *neuron_filename,
    const char *population_filename,
    ExperimentResult *result)
{
    Network net;
    NeuronRecorder neuron_recorder = {NULL, -1};
    PopulationRecorder population_recorder = {NULL};
    int net_initialized = 0;

    reset_result(result);

    if (!network_init(&net, 3))
    {
        printf("Erro: falha ao criar rede do cenario %s.\n", label);
        return 0;
    }

    net_initialized = 1;

    if (!configure_common_network(&net))
    {
        printf("Erro: falha ao configurar tipos no cenario %s.\n", label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!network_connect_delayed(&net, SOURCE_EXC, TARGET, W_EXC, 1))
    {
        printf("Erro: falha ao criar sinapse excitatoria no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (balanced &&
        !network_connect_delayed(&net, SOURCE_INH, TARGET, W_INH, 1))
    {
        printf("Erro: falha ao criar sinapse inhibitoria no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!neuron_recorder_open(&neuron_recorder, neuron_filename, TARGET))
    {
        printf("Erro: falha ao abrir recorder de neuronio no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!neuron_recorder_write_header(&neuron_recorder))
    {
        printf("Erro: falha ao escrever cabecalho de neuronio no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!population_recorder_open(&population_recorder, population_filename))
    {
        printf("Erro: falha ao abrir recorder de populacao no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!population_recorder_write_header(&population_recorder))
    {
        printf("Erro: falha ao escrever cabecalho de populacao no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    for (int step = 0; step < EXPERIMENT_STEPS; step++)
    {
        network_clear_external_currents(&net);

        if (!network_set_external_current(&net, SOURCE_EXC, I_EXT))
        {
            printf("Erro: falha ao aplicar corrente EXC no cenario %s.\n",
                   label);
            close_resources(
                &net,
                &neuron_recorder,
                &population_recorder,
                net_initialized);
            return 0;
        }

        if (balanced &&
            !network_set_external_current(&net, SOURCE_INH, I_EXT))
        {
            printf("Erro: falha ao aplicar corrente INH no cenario %s.\n",
                   label);
            close_resources(
                &net,
                &neuron_recorder,
                &population_recorder,
                net_initialized);
            return 0;
        }

        network_update(&net);

        if (result->first_exc_spike_step < 0 && net.spikes[SOURCE_EXC])
            result->first_exc_spike_step = step;

        if (balanced &&
            result->first_inh_spike_step < 0 &&
            net.spikes[SOURCE_INH])
        {
            result->first_inh_spike_step = step;
        }

        if (result->first_target_current_step < 0 &&
            is_nonzero(net.used_syn_current[TARGET]))
        {
            result->first_target_current_step = step;
            result->first_target_current = net.used_syn_current[TARGET];
        }

        if (net.neurons[TARGET].V > result->max_target_potential)
            result->max_target_potential = net.neurons[TARGET].V;

        if (net.neurons[TARGET].V < result->min_target_potential)
            result->min_target_potential = net.neurons[TARGET].V;

        if (!neuron_recorder_record(&neuron_recorder, &net, step))
        {
            printf("Erro: falha ao registrar neuronio no cenario %s.\n",
                   label);
            close_resources(
                &net,
                &neuron_recorder,
                &population_recorder,
                net_initialized);
            return 0;
        }

        if (!population_recorder_record(&population_recorder, &net, step))
        {
            printf("Erro: falha ao registrar populacao no cenario %s.\n",
                   label);
            close_resources(
                &net,
                &neuron_recorder,
                &population_recorder,
                net_initialized);
            return 0;
        }
    }

    if (result->first_exc_spike_step < 0)
    {
        printf("Erro: neuronio 0 nunca disparou no cenario %s.\n", label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (balanced && result->first_inh_spike_step < 0)
    {
        printf("Erro: neuronio 1 nunca disparou no cenario %s.\n", label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (result->first_target_current_step < 0)
    {
        printf("Erro: alvo nunca recebeu corrente no cenario %s.\n", label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!balanced && result->first_target_current <= 0.0)
    {
        printf("Erro: alvo nao recebeu corrente positiva no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (balanced && result->first_target_current >= 0.0)
    {
        printf("Erro: alvo nao recebeu corrente negativa no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (!balanced && result->max_target_potential <= V_REST)
    {
        printf("Erro: alvo nao subiu acima de V_REST no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    if (balanced && result->min_target_potential >= V_REST)
    {
        printf("Erro: alvo nao caiu abaixo de V_REST no cenario %s.\n",
               label);
        close_resources(
            &net,
            &neuron_recorder,
            &population_recorder,
            net_initialized);
        return 0;
    }

    close_resources(
        &net,
        &neuron_recorder,
        &population_recorder,
        net_initialized);

    return 1;
}

int main(void)
{
    ExperimentResult exc_only;
    ExperimentResult balanced;

    if (!run_scenario(
            "EXC-only",
            0,
            "results/experiments/ei_balance/ei_exc_only_neuron2.csv",
            "results/experiments/ei_balance/ei_exc_only_population.csv",
            &exc_only))
    {
        return 1;
    }

    if (!run_scenario(
            "EXC/INH",
            1,
            "results/experiments/ei_balance/ei_balanced_neuron2.csv",
            "results/experiments/ei_balance/ei_balanced_population.csv",
            &balanced))
    {
        return 1;
    }

    printf("=== Experimento EXC vs EXC/INH ===\n");
    printf("EXC-only:\n");
    printf("  primeiro spike da fonte: %d\n",
           exc_only.first_exc_spike_step);
    printf("  primeira corrente no alvo: t=%d, I=%.2f\n",
           exc_only.first_target_current_step,
           exc_only.first_target_current);
    printf("  maior V do alvo: %.2f\n",
           exc_only.max_target_potential);
    printf("\n");
    printf("EXC/INH:\n");
    printf("  primeiro spike EXC: %d\n",
           balanced.first_exc_spike_step);
    printf("  primeiro spike INH: %d\n",
           balanced.first_inh_spike_step);
    printf("  primeira corrente no alvo: t=%d, I=%.2f\n",
           balanced.first_target_current_step,
           balanced.first_target_current);
    printf("  menor V do alvo: %.2f\n",
           balanced.min_target_potential);
    printf("\n");
    printf("Resultado: efeito inibitorio confirmado.\n");

    return 0;
}

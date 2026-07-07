#include <stdio.h>

#include "recorder.h"

static void neuron_recorder_reset(NeuronRecorder *recorder)
{
    if (recorder == NULL)
        return;

    recorder->file = NULL;
    recorder->neuron_id = -1;
}

static void population_recorder_reset(PopulationRecorder *recorder)
{
    if (recorder == NULL)
        return;

    recorder->file = NULL;
}

int neuron_recorder_open(
    NeuronRecorder *recorder,
    const char *filename,
    int neuron_id)
{
    if (recorder == NULL)
        return 0;

    neuron_recorder_reset(recorder);

    if (filename == NULL || neuron_id < 0)
        return 0;

    recorder->file = fopen(filename, "w");

    if (recorder->file == NULL)
    {
        neuron_recorder_reset(recorder);
        return 0;
    }

    recorder->neuron_id = neuron_id;
    return 1;
}

int neuron_recorder_write_header(NeuronRecorder *recorder)
{
    if (recorder == NULL || recorder->file == NULL)
        return 0;

    if (fprintf(
            recorder->file,
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n") < 0)
    {
        return 0;
    }

    return 1;
}

int neuron_recorder_record(
    NeuronRecorder *recorder,
    const Network *net,
    int step)
{
    int neuron_id;

    if (recorder == NULL || recorder->file == NULL ||
        net == NULL || step < 0)
    {
        return 0;
    }

    neuron_id = recorder->neuron_id;

    if (neuron_id < 0 || neuron_id >= net->size)
        return 0;

    if (net->neurons == NULL ||
        net->spikes == NULL ||
        net->ext_current == NULL ||
        net->syn_current == NULL ||
        net->used_syn_current == NULL)
    {
        return 0;
    }

    if (fprintf(
            recorder->file,
            "%d,%.2f,%d,%.2f,%.2f\n",
            step,
            net->neurons[neuron_id].V,
            net->spikes[neuron_id],
            net->ext_current[neuron_id],
            net->used_syn_current[neuron_id]) < 0)
    {
        return 0;
    }

    return 1;
}

void neuron_recorder_close(NeuronRecorder *recorder)
{
    if (recorder == NULL)
        return;

    if (recorder->file != NULL)
        fclose(recorder->file);

    neuron_recorder_reset(recorder);
}

int population_recorder_open(
    PopulationRecorder *recorder,
    const char *filename)
{
    if (recorder == NULL)
        return 0;

    population_recorder_reset(recorder);

    if (filename == NULL)
        return 0;

    recorder->file = fopen(filename, "w");

    if (recorder->file == NULL)
    {
        population_recorder_reset(recorder);
        return 0;
    }

    return 1;
}

int population_recorder_write_header(
    PopulationRecorder *recorder)
{
    if (recorder == NULL || recorder->file == NULL)
        return 0;

    if (fprintf(
            recorder->file,
            "tempo,spikes_total,spikes_exc,spikes_inh,"
            "mean_potential,mean_syn_current\n") < 0)
    {
        return 0;
    }

    return 1;
}

int population_recorder_record(
    PopulationRecorder *recorder,
    const Network *net,
    int step)
{
    int spikes_total = 0;
    int spikes_exc = 0;
    int spikes_inh = 0;
    double potential_sum = 0.0;
    double syn_current_sum = 0.0;

    if (recorder == NULL || recorder->file == NULL ||
        net == NULL || step < 0)
    {
        return 0;
    }

    if (net->size <= 0 ||
        net->neurons == NULL ||
        net->spikes == NULL ||
        net->used_syn_current == NULL)
    {
        return 0;
    }

    for (int i = 0; i < net->size; i++)
    {
        int spike = net->spikes[i];
        NeuronType type = net->neurons[i].type;

        if (spike != 0 && spike != 1)
            return 0;

        if (type != NEURON_EXCITATORY &&
            type != NEURON_INHIBITORY)
        {
            return 0;
        }

        spikes_total += spike;

        if (type == NEURON_EXCITATORY)
            spikes_exc += spike;
        else
            spikes_inh += spike;

        potential_sum += net->neurons[i].V;
        syn_current_sum += net->used_syn_current[i];
    }

    if (fprintf(
            recorder->file,
            "%d,%d,%d,%d,%.2f,%.2f\n",
            step,
            spikes_total,
            spikes_exc,
            spikes_inh,
            potential_sum / (double)net->size,
            syn_current_sum / (double)net->size) < 0)
    {
        return 0;
    }

    return 1;
}

void population_recorder_close(
    PopulationRecorder *recorder)
{
    if (recorder == NULL)
        return;

    if (recorder->file != NULL)
        fclose(recorder->file);

    population_recorder_reset(recorder);
}

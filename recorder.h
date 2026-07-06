#ifndef RECORDER_H
#define RECORDER_H

#include <stdio.h>

#include "network.h"

typedef struct
{
    FILE *file;
    int neuron_id;
} NeuronRecorder;

typedef struct
{
    FILE *file;
} PopulationRecorder;

int neuron_recorder_open(
    NeuronRecorder *recorder,
    const char *filename,
    int neuron_id);

int neuron_recorder_write_header(NeuronRecorder *recorder);

int neuron_recorder_record(
    NeuronRecorder *recorder,
    const Network *net,
    int step);

void neuron_recorder_close(NeuronRecorder *recorder);

int population_recorder_open(
    PopulationRecorder *recorder,
    const char *filename);

int population_recorder_write_header(
    PopulationRecorder *recorder);

int population_recorder_record(
    PopulationRecorder *recorder,
    const Network *net,
    int step);

void population_recorder_close(
    PopulationRecorder *recorder);

#endif

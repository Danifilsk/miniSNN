#ifndef STIMULUS_H
#define STIMULUS_H

#include "network.h"

typedef struct
{
    int neuron_id;
    int start_step;
    int end_step;
    double current;
} PulseStimulus;

typedef struct
{
    PulseStimulus *pulses;
    int count;
    int capacity;
} StimulusSchedule;

int pulse_stimulus_init(
    PulseStimulus *pulse,
    int neuron_id,
    int start_step,
    int end_step,
    double current);

int pulse_stimulus_apply(
    const PulseStimulus *pulse,
    Network *net,
    int step);

int stimulus_schedule_init(StimulusSchedule *schedule);

void stimulus_schedule_destroy(StimulusSchedule *schedule);

int stimulus_schedule_add_pulse(
    StimulusSchedule *schedule,
    int neuron_id,
    int start_step,
    int end_step,
    double current);

int stimulus_schedule_apply(
    const StimulusSchedule *schedule,
    Network *net,
    int step);

#endif

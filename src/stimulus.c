#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include "stimulus.h"

static int pulse_stimulus_is_valid(const PulseStimulus *pulse)
{
    if (pulse == NULL)
        return 0;

    if (pulse->neuron_id < 0)
        return 0;

    if (pulse->start_step < 0)
        return 0;

    if (pulse->end_step <= pulse->start_step)
        return 0;

    if (!isfinite(pulse->current))
        return 0;

    return 1;
}

static int stimulus_schedule_is_valid(const StimulusSchedule *schedule)
{
    if (schedule == NULL)
        return 0;

    if (schedule->count < 0 || schedule->capacity < 0)
        return 0;

    if (schedule->count > schedule->capacity)
        return 0;

    if (schedule->count > 0 && schedule->pulses == NULL)
        return 0;

    if (schedule->capacity > 0 && schedule->pulses == NULL)
        return 0;

    return 1;
}

int pulse_stimulus_init(
    PulseStimulus *pulse,
    int neuron_id,
    int start_step,
    int end_step,
    double current)
{
    if (pulse == NULL)
        return 0;

    if (neuron_id < 0)
        return 0;

    if (start_step < 0)
        return 0;

    if (end_step <= start_step)
        return 0;

    if (!isfinite(current))
        return 0;

    pulse->neuron_id = neuron_id;
    pulse->start_step = start_step;
    pulse->end_step = end_step;
    pulse->current = current;

    return 1;
}

int pulse_stimulus_apply(
    const PulseStimulus *pulse,
    Network *net,
    int step)
{
    if (pulse == NULL || net == NULL || step < 0)
        return 0;

    if (!pulse_stimulus_is_valid(pulse))
        return 0;

    if (pulse->neuron_id >= net->size)
        return 0;

    if (step < pulse->start_step || step >= pulse->end_step)
        return 1;

    return network_add_external_current(net, pulse->neuron_id, pulse->current);
}

int stimulus_schedule_init(StimulusSchedule *schedule)
{
    if (schedule == NULL)
        return 0;

    schedule->pulses = NULL;
    schedule->count = 0;
    schedule->capacity = 0;

    return 1;
}

void stimulus_schedule_destroy(StimulusSchedule *schedule)
{
    if (schedule == NULL)
        return;

    free(schedule->pulses);
    schedule->pulses = NULL;
    schedule->count = 0;
    schedule->capacity = 0;
}

int stimulus_schedule_add_pulse(
    StimulusSchedule *schedule,
    int neuron_id,
    int start_step,
    int end_step,
    double current)
{
    PulseStimulus pulse;

    if (!stimulus_schedule_is_valid(schedule))
        return 0;

    if (!pulse_stimulus_init(
            &pulse,
            neuron_id,
            start_step,
            end_step,
            current))
    {
        return 0;
    }

    if (schedule->count == schedule->capacity)
    {
        int new_capacity = 4;

        if (schedule->capacity > 0)
        {
            if (schedule->capacity > INT_MAX / 2)
                return 0;

            new_capacity = schedule->capacity * 2;
        }

        if (new_capacity <= schedule->capacity ||
            new_capacity < schedule->count + 1)
        {
            return 0;
        }

        PulseStimulus *new_pulses = realloc(
            schedule->pulses,
            (size_t)new_capacity * sizeof(PulseStimulus));

        if (new_pulses == NULL)
            return 0;

        schedule->pulses = new_pulses;
        schedule->capacity = new_capacity;
    }

    schedule->pulses[schedule->count] = pulse;
    schedule->count++;

    return 1;
}

int stimulus_schedule_apply(
    const StimulusSchedule *schedule,
    Network *net,
    int step)
{
    if (!stimulus_schedule_is_valid(schedule))
        return 0;

    if (net == NULL || step < 0)
        return 0;

    for (int i = 0; i < schedule->count; i++)
    {
        if (!pulse_stimulus_apply(&schedule->pulses[i], net, step))
            return 0;
    }

    return 1;
}

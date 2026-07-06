#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "network.h"
#include "recorder.h"
#include "stimulus.h"
#include "topology.h"

static int same_double(double a, double b)
{
    double diff = a - b;
    return diff > -1e-12 && diff < 1e-12;
}

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
}

static int check_initial_state(int size)
{
    Network net;

    if (!network_init(&net, size))
        return fail("network_init failed");

    if (net.max_synaptic_delay != MAX_SYNAPTIC_DELAY ||
        net.delay_cursor != 0)
    {
        network_destroy(&net);
        return fail("delay queue metadata did not start correctly");
    }

    for (int i = 0; i < net.size; i++)
    {
        if (!same_double(net.neurons[i].V, V_REST))
        {
            network_destroy(&net);
            return fail("neuron did not start at V_REST");
        }

        if (net.neurons[i].spike != 0 || net.spikes[i] != 0)
        {
            network_destroy(&net);
            return fail("spike state did not start at 0");
        }

        if (net.neurons[i].type != NEURON_EXCITATORY)
        {
            network_destroy(&net);
            return fail("neuron did not start as excitatory");
        }

        if (!same_double(net.syn_current[i], 0.0) ||
            !same_double(net.used_syn_current[i], 0.0) ||
            !same_double(net.ext_current[i], 0.0))
        {
            network_destroy(&net);
            return fail("currents did not start at 0");
        }

        if (net.connections[i].list != NULL || net.connections[i].count != 0)
        {
            network_destroy(&net);
            return fail("connection list did not start empty");
        }
    }

    for (int i = 0; i < net.size * net.max_synaptic_delay; i++)
    {
        if (!same_double(net.pending_current[i], 0.0))
        {
            network_destroy(&net);
            return fail("pending_current did not start at 0");
        }
    }

    network_destroy(&net);
    return 1;
}

static int check_runtime_state(Network *net, int steps)
{
    for (int step = 0; step < steps; step++)
    {
        int total_spikes = network_update(net);
        int counted_spikes = 0;

        for (int i = 0; i < net->size; i++)
        {
            if (net->spikes[i] != 0 && net->spikes[i] != 1)
                return fail("spike value was not 0 or 1");

            counted_spikes += net->spikes[i];

            if (net->neurons[i].V < -1000000.0 ||
                net->neurons[i].V > 1000000.0)
            {
                return fail("membrane potential became absurd");
            }

            if (net->delay_cursor < 0 ||
                net->delay_cursor >= net->max_synaptic_delay)
            {
                return fail("delay cursor is out of range");
            }

            if (!same_double(
                    net->pending_current[
                        net->delay_cursor * net->size + i],
                    0.0))
            {
                return fail("current delay slot was not cleared");
            }
        }

        if (counted_spikes != total_spikes)
            return fail("network_update returned an inconsistent spike count");
    }

    return 1;
}

static int has_target(ConnectionList *connections, int target)
{
    for (int i = 0; i < connections->count; i++)
    {
        if (connections->list[i].target == target)
            return 1;
    }

    return 0;
}

static int check_no_duplicate_targets(ConnectionList *connections)
{
    for (int i = 0; i < connections->count; i++)
    {
        for (int j = i + 1; j < connections->count; j++)
        {
            if (connections->list[i].target == connections->list[j].target)
                return 0;
        }
    }

    return 1;
}

static int all_neurons_have_type(Network *net, NeuronType type)
{
    for (int i = 0; i < net->size; i++)
    {
        if (net->neurons[i].type != type)
            return 0;
    }

    return 1;
}

static int count_neurons_with_type(Network *net, NeuronType type)
{
    int count = 0;

    for (int i = 0; i < net->size; i++)
    {
        if (net->neurons[i].type == type)
            count++;
    }

    return count;
}

static int same_topology_state(Network *a, Network *b)
{
    if (a->size != b->size)
        return 0;

    for (int i = 0; i < a->size; i++)
    {
        if (a->neurons[i].type != b->neurons[i].type)
            return 0;

        if (a->connections[i].count != b->connections[i].count)
            return 0;

        for (int j = 0; j < a->connections[i].count; j++)
        {
            if (a->connections[i].list[j].target !=
                b->connections[i].list[j].target)
            {
                return 0;
            }

            if (!same_double(
                    a->connections[i].list[j].weight,
                    b->connections[i].list[j].weight))
            {
                return 0;
            }

            if (a->connections[i].list[j].delay !=
                b->connections[i].list[j].delay)
            {
                return 0;
            }
        }
    }

    return 1;
}

static int all_connections_have_delay(Network *net, int delay)
{
    for (int i = 0; i < net->size; i++)
    {
        for (int j = 0; j < net->connections[i].count; j++)
        {
            if (net->connections[i].list[j].delay != delay)
                return 0;
        }
    }

    return 1;
}

static int apply_test_current(Network *net)
{
    if (!network_set_external_current(net, 0, I_EXT))
        return fail("network_set_external_current rejected I_EXT");

    return 1;
}

static int check_neuron_type_api(void)
{
    Network net;

    if (!network_init(&net, 4))
        return fail("network_init failed for neuron type API");

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("not all neurons started excitatory");
    }

    if (!network_set_neuron_type(&net, 1, NEURON_INHIBITORY))
    {
        network_destroy(&net);
        return fail("valid inhibitory type was rejected");
    }

    if (net.neurons[1].type != NEURON_INHIBITORY)
    {
        network_destroy(&net);
        return fail("inhibitory type was not stored");
    }

    if (!network_set_neuron_type(&net, 1, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("valid excitatory type was rejected");
    }

    if (net.neurons[1].type != NEURON_EXCITATORY)
    {
        network_destroy(&net);
        return fail("excitatory type was not stored");
    }

    if (!network_set_neuron_type(&net, 2, NEURON_INHIBITORY))
    {
        network_destroy(&net);
        return fail("setup failed for invalid neuron type calls");
    }

    if (network_set_neuron_type(&net, -1, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("negative neuron type index was accepted");
    }

    if (network_set_neuron_type(&net, net.size, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("too-large neuron type index was accepted");
    }

    if (network_set_neuron_type(&net, 2, (NeuronType)99))
    {
        network_destroy(&net);
        return fail("invalid neuron type was accepted");
    }

    if (network_set_neuron_type(NULL, 0, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("NULL network was accepted for neuron type");
    }

    if (net.neurons[2].type != NEURON_INHIBITORY)
    {
        network_destroy(&net);
        return fail("invalid type call changed previous type");
    }

    network_destroy(&net);
    return 1;
}

static int check_external_current_api(void)
{
    Network net;

    if (!network_init(&net, 3))
        return fail("network_init failed for external currents");

    if (!network_set_external_current(&net, 1, 3.0))
    {
        network_destroy(&net);
        return fail("valid external current was rejected");
    }

    if (!same_double(net.ext_current[1], 3.0))
    {
        network_destroy(&net);
        return fail("external current was not set");
    }

    if (!network_add_external_current(&net, 1, 2.5))
    {
        network_destroy(&net);
        return fail("valid external current addition was rejected");
    }

    if (!same_double(net.ext_current[1], 5.5))
    {
        network_destroy(&net);
        return fail("external current was not added");
    }

    if (network_set_external_current(&net, -1, 1.0))
    {
        network_destroy(&net);
        return fail("negative external current index was accepted");
    }

    if (network_set_external_current(&net, net.size, 1.0))
    {
        network_destroy(&net);
        return fail("too-large external current index was accepted");
    }

    if (network_add_external_current(&net, -1, 1.0))
    {
        network_destroy(&net);
        return fail("negative add current index was accepted");
    }

    if (network_add_external_current(&net, net.size, 1.0))
    {
        network_destroy(&net);
        return fail("too-large add current index was accepted");
    }

    if (network_set_external_current(&net, 1, NAN) ||
        network_set_external_current(&net, 1, INFINITY) ||
        network_set_external_current(&net, 1, -INFINITY))
    {
        network_destroy(&net);
        return fail("non-finite set current was accepted");
    }

    if (!same_double(net.ext_current[1], 5.5))
    {
        network_destroy(&net);
        return fail("invalid set changed the external current");
    }

    if (network_add_external_current(&net, 1, NAN) ||
        network_add_external_current(&net, 1, INFINITY) ||
        network_add_external_current(&net, 1, -INFINITY))
    {
        network_destroy(&net);
        return fail("non-finite added current was accepted");
    }

    if (!same_double(net.ext_current[1], 5.5))
    {
        network_destroy(&net);
        return fail("invalid add changed the external current");
    }

    if (!network_set_external_current(&net, 2, DBL_MAX))
    {
        network_destroy(&net);
        return fail("large finite external current was rejected");
    }

    if (network_add_external_current(&net, 2, DBL_MAX))
    {
        network_destroy(&net);
        return fail("non-finite external current sum was accepted");
    }

    if (!same_double(net.ext_current[2], DBL_MAX))
    {
        network_destroy(&net);
        return fail("invalid sum changed the external current");
    }

    network_clear_external_currents(&net);
    network_clear_external_currents(NULL);

    for (int i = 0; i < net.size; i++)
    {
        if (!same_double(net.ext_current[i], 0.0))
        {
            network_destroy(&net);
            return fail("external current was not cleared");
        }
    }

    network_destroy(&net);
    return 1;
}

static int check_pulse_stimulus_api(void)
{
    Network net;
    PulseStimulus pulse;
    PulseStimulus second_pulse;
    PulseStimulus negative_pulse;
    PulseStimulus missing_neuron_pulse;
    PulseStimulus unchanged_pulse = {1, 2, 3, 4.0};

    if (!network_init(&net, 3))
        return fail("network_init failed for pulse stimulus");

    if (!pulse_stimulus_init(&pulse, 1, 10, 20, 2.0))
    {
        network_destroy(&net);
        return fail("valid pulse was rejected");
    }

    if (pulse.neuron_id != 1 ||
        pulse.start_step != 10 ||
        pulse.end_step != 20 ||
        !same_double(pulse.current, 2.0))
    {
        network_destroy(&net);
        return fail("valid pulse was not stored correctly");
    }

    if (pulse_stimulus_init(&unchanged_pulse, 1, -1, 20, 2.0))
    {
        network_destroy(&net);
        return fail("pulse with negative start was accepted");
    }

    if (unchanged_pulse.neuron_id != 1 ||
        unchanged_pulse.start_step != 2 ||
        unchanged_pulse.end_step != 3 ||
        !same_double(unchanged_pulse.current, 4.0))
    {
        network_destroy(&net);
        return fail("failed pulse init changed the original pulse");
    }

    if (pulse_stimulus_init(&unchanged_pulse, 1, 10, 10, 2.0) ||
        pulse_stimulus_init(&unchanged_pulse, 1, 10, 9, 2.0))
    {
        network_destroy(&net);
        return fail("pulse with invalid end was accepted");
    }

    if (pulse_stimulus_init(&unchanged_pulse, 1, 10, 20, NAN) ||
        pulse_stimulus_init(&unchanged_pulse, 1, 10, 20, INFINITY) ||
        pulse_stimulus_init(&unchanged_pulse, 1, 10, 20, -INFINITY))
    {
        network_destroy(&net);
        return fail("pulse with non-finite current was accepted");
    }

    if (pulse_stimulus_init(NULL, 1, 10, 20, 2.0))
    {
        network_destroy(&net);
        return fail("NULL pulse init was accepted");
    }

    network_clear_external_currents(&net);

    if (!network_set_external_current(&net, 1, 4.0))
    {
        network_destroy(&net);
        return fail("setup failed for inactive pulse");
    }

    if (!pulse_stimulus_apply(&pulse, &net, 9))
    {
        network_destroy(&net);
        return fail("inactive pulse before start failed");
    }

    if (!same_double(net.ext_current[1], 4.0))
    {
        network_destroy(&net);
        return fail("inactive pulse before start changed current");
    }

    network_clear_external_currents(&net);

    if (!pulse_stimulus_apply(&pulse, &net, 10) ||
        !same_double(net.ext_current[1], 2.0))
    {
        network_destroy(&net);
        return fail("pulse did not apply at start_step");
    }

    network_clear_external_currents(&net);

    if (!pulse_stimulus_apply(&pulse, &net, 19) ||
        !same_double(net.ext_current[1], 2.0))
    {
        network_destroy(&net);
        return fail("pulse did not apply at last valid step");
    }

    network_clear_external_currents(&net);

    if (!pulse_stimulus_apply(&pulse, &net, 20))
    {
        network_destroy(&net);
        return fail("inactive pulse at end_step failed");
    }

    if (!same_double(net.ext_current[1], 0.0))
    {
        network_destroy(&net);
        return fail("pulse applied at end_step");
    }

    if (!pulse_stimulus_init(&second_pulse, 1, 10, 20, 3.0))
    {
        network_destroy(&net);
        return fail("second valid pulse was rejected");
    }

    network_clear_external_currents(&net);

    if (!pulse_stimulus_apply(&pulse, &net, 10) ||
        !pulse_stimulus_apply(&second_pulse, &net, 10) ||
        !same_double(net.ext_current[1], 5.0))
    {
        network_destroy(&net);
        return fail("active pulses did not sum currents");
    }

    if (!pulse_stimulus_init(&negative_pulse, 1, 10, 20, -1.5))
    {
        network_destroy(&net);
        return fail("negative pulse was rejected");
    }

    network_clear_external_currents(&net);

    if (!network_set_external_current(&net, 1, 5.0) ||
        !pulse_stimulus_apply(&negative_pulse, &net, 10) ||
        !same_double(net.ext_current[1], 3.5))
    {
        network_destroy(&net);
        return fail("negative pulse did not reduce current");
    }

    if (!pulse_stimulus_init(&missing_neuron_pulse, 5, 10, 20, 2.0))
    {
        network_destroy(&net);
        return fail("pulse for future network neuron was rejected at init");
    }

    network_clear_external_currents(&net);

    if (!network_set_external_current(&net, 1, 7.0))
    {
        network_destroy(&net);
        return fail("setup failed for missing neuron pulse");
    }

    if (pulse_stimulus_apply(&missing_neuron_pulse, &net, 10))
    {
        network_destroy(&net);
        return fail("pulse for missing neuron was accepted");
    }

    if (!same_double(net.ext_current[1], 7.0))
    {
        network_destroy(&net);
        return fail("missing neuron pulse changed the network");
    }

    if (pulse_stimulus_apply(NULL, &net, 10) ||
        pulse_stimulus_apply(&pulse, NULL, 10) ||
        pulse_stimulus_apply(&pulse, &net, -1))
    {
        network_destroy(&net);
        return fail("invalid pulse apply call was accepted");
    }

    network_destroy(&net);
    return 1;
}

static int check_stimulus_schedule_api(void)
{
    Network net;
    StimulusSchedule schedule;
    StimulusSchedule empty_schedule;
    StimulusSchedule missing_neuron_schedule;
    StimulusSchedule bad_count = {NULL, -1, 0};
    StimulusSchedule bad_capacity = {NULL, 0, -1};
    StimulusSchedule bad_order = {NULL, 2, 1};
    StimulusSchedule bad_missing_pulses = {NULL, 1, 4};

    if (!network_init(&net, 4))
        return fail("network_init failed for stimulus schedule");

    if (!stimulus_schedule_init(&schedule))
    {
        network_destroy(&net);
        return fail("valid schedule init was rejected");
    }

    if (schedule.pulses != NULL ||
        schedule.count != 0 ||
        schedule.capacity != 0)
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule init did not clear fields");
    }

    stimulus_schedule_destroy(NULL);

    if (!stimulus_schedule_add_pulse(&schedule, 1, 5, 10, 2.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("valid schedule pulse was rejected");
    }

    if (schedule.count != 1 ||
        schedule.capacity < 1 ||
        schedule.pulses == NULL ||
        schedule.pulses[0].neuron_id != 1 ||
        schedule.pulses[0].start_step != 5 ||
        schedule.pulses[0].end_step != 10 ||
        !same_double(schedule.pulses[0].current, 2.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule did not store first pulse correctly");
    }

    int old_count = schedule.count;
    int old_capacity = schedule.capacity;
    PulseStimulus *old_pulses = schedule.pulses;
    PulseStimulus old_first_pulse = schedule.pulses[0];

    if (stimulus_schedule_add_pulse(&schedule, 1, -1, 10, 2.0) ||
        stimulus_schedule_add_pulse(&schedule, 1, 10, 10, 2.0) ||
        stimulus_schedule_add_pulse(&schedule, 1, 5, 10, NAN))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("invalid schedule pulse was accepted");
    }

    if (schedule.count != old_count ||
        schedule.capacity != old_capacity ||
        schedule.pulses != old_pulses ||
        schedule.pulses[0].neuron_id != old_first_pulse.neuron_id ||
        schedule.pulses[0].start_step != old_first_pulse.start_step ||
        schedule.pulses[0].end_step != old_first_pulse.end_step ||
        !same_double(schedule.pulses[0].current, old_first_pulse.current))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("failed schedule add changed existing data");
    }

    for (int i = 0; i < 4; i++)
    {
        if (!stimulus_schedule_add_pulse(
                &schedule,
                i % net.size,
                i,
                i + 3,
                1.0 + i))
        {
            stimulus_schedule_destroy(&schedule);
            network_destroy(&net);
            return fail("adding several schedule pulses failed");
        }
    }

    if (schedule.count != 5 || schedule.capacity < 5)
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule capacity did not grow");
    }

    if (schedule.pulses[4].neuron_id != 3 ||
        schedule.pulses[4].start_step != 3 ||
        schedule.pulses[4].end_step != 6 ||
        !same_double(schedule.pulses[4].current, 4.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule did not store later pulse correctly");
    }

    if (!stimulus_schedule_add_pulse(&schedule, 2, 4, 7, 1.5))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("adding overlapping schedule pulse failed");
    }

    network_clear_external_currents(&net);

    if (!stimulus_schedule_apply(&schedule, &net, 4))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("valid schedule apply failed");
    }

    if (!same_double(net.ext_current[0], 0.0) ||
        !same_double(net.ext_current[1], 0.0) ||
        !same_double(net.ext_current[2], 4.5) ||
        !same_double(net.ext_current[3], 4.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule did not apply pulses to expected neurons");
    }

    network_clear_external_currents(&net);

    if (!network_set_external_current(&net, 0, 9.0) ||
        !stimulus_schedule_apply(&schedule, &net, 20) ||
        !same_double(net.ext_current[0], 9.0) ||
        !same_double(net.ext_current[1], 0.0) ||
        !same_double(net.ext_current[2], 0.0) ||
        !same_double(net.ext_current[3], 0.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("inactive schedule changed the network");
    }

    if (!stimulus_schedule_init(&empty_schedule))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("empty schedule init failed");
    }

    if (!network_set_external_current(&net, 0, 9.0) ||
        !stimulus_schedule_apply(&empty_schedule, &net, 4) ||
        !same_double(net.ext_current[0], 9.0))
    {
        stimulus_schedule_destroy(&empty_schedule);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("empty schedule changed the network");
    }

    stimulus_schedule_destroy(&empty_schedule);

    if (stimulus_schedule_apply(NULL, &net, 4) ||
        stimulus_schedule_apply(&schedule, NULL, 4) ||
        stimulus_schedule_apply(&schedule, &net, -1))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("invalid schedule apply call was accepted");
    }

    if (stimulus_schedule_add_pulse(NULL, 0, 1, 2, 3.0) ||
        stimulus_schedule_add_pulse(&bad_count, 0, 1, 2, 3.0) ||
        stimulus_schedule_add_pulse(&bad_capacity, 0, 1, 2, 3.0) ||
        stimulus_schedule_add_pulse(&bad_order, 0, 1, 2, 3.0) ||
        stimulus_schedule_add_pulse(&bad_missing_pulses, 0, 1, 2, 3.0))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("inconsistent schedule add was accepted");
    }

    if (stimulus_schedule_apply(&bad_count, &net, 4) ||
        stimulus_schedule_apply(&bad_capacity, &net, 4) ||
        stimulus_schedule_apply(&bad_order, &net, 4) ||
        stimulus_schedule_apply(&bad_missing_pulses, &net, 4))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("inconsistent schedule apply was accepted");
    }

    if (!stimulus_schedule_init(&missing_neuron_schedule))
    {
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("missing-neuron schedule init failed");
    }

    if (!stimulus_schedule_add_pulse(&missing_neuron_schedule, 8, 4, 6, 1.0))
    {
        stimulus_schedule_destroy(&missing_neuron_schedule);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule pulse for future network neuron was rejected at add");
    }

    network_clear_external_currents(&net);

    if (!network_set_external_current(&net, 0, 11.0))
    {
        stimulus_schedule_destroy(&missing_neuron_schedule);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("setup failed for bad schedule pulse");
    }

    if (stimulus_schedule_apply(&missing_neuron_schedule, &net, 4))
    {
        stimulus_schedule_destroy(&missing_neuron_schedule);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("schedule apply accepted missing neuron pulse");
    }

    if (!same_double(net.ext_current[0], 11.0))
    {
        stimulus_schedule_destroy(&missing_neuron_schedule);
        stimulus_schedule_destroy(&schedule);
        network_destroy(&net);
        return fail("failed schedule apply changed unexpected current");
    }

    stimulus_schedule_destroy(&missing_neuron_schedule);
    stimulus_schedule_destroy(&schedule);

    if (schedule.pulses != NULL ||
        schedule.count != 0 ||
        schedule.capacity != 0)
    {
        network_destroy(&net);
        return fail("schedule destroy did not reset fields");
    }

    network_destroy(&net);
    return 1;
}

static int check_neuron_recorder_api(void)
{
    const char *filename = "test_neuron_recorder.csv";
    char header[128];
    char row[128];
    FILE *file;
    Network net;
    NeuronRecorder recorder;
    NeuronRecorder invalid_recorder;

    remove(filename);

    if (neuron_recorder_open(NULL, filename, 0))
        return fail("NULL recorder open was accepted");

    invalid_recorder.file = NULL;
    invalid_recorder.neuron_id = 123;

    if (neuron_recorder_open(&invalid_recorder, NULL, 0))
        return fail("NULL filename recorder open was accepted");

    if (invalid_recorder.file != NULL || invalid_recorder.neuron_id != -1)
        return fail("failed NULL filename open did not reset recorder");

    if (neuron_recorder_open(&invalid_recorder, filename, -1))
        return fail("negative recorder neuron was accepted");

    if (invalid_recorder.file != NULL || invalid_recorder.neuron_id != -1)
        return fail("failed negative recorder open did not reset recorder");

    if (!network_init(&net, 2))
        return fail("network_init failed for recorder");

    if (!network_set_external_current(&net, 0, I_EXT))
    {
        network_destroy(&net);
        return fail("recorder test current setup failed");
    }

    network_update(&net);

    double old_v = net.neurons[0].V;
    int old_spike = net.spikes[0];
    double old_ext_current = net.ext_current[0];
    double old_syn_current = net.syn_current[0];
    double old_used_syn_current = net.used_syn_current[0];
    int old_step = net.step;

    if (!neuron_recorder_open(&recorder, filename, 0))
    {
        network_destroy(&net);
        return fail("valid recorder open failed");
    }

    if (!neuron_recorder_write_header(&recorder))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("valid recorder header failed");
    }

    if (!neuron_recorder_record(&recorder, &net, 0))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("valid recorder row failed");
    }

    if (!same_double(net.neurons[0].V, old_v) ||
        net.spikes[0] != old_spike ||
        !same_double(net.ext_current[0], old_ext_current) ||
        !same_double(net.syn_current[0], old_syn_current) ||
        !same_double(net.used_syn_current[0], old_used_syn_current) ||
        net.step != old_step)
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder changed network state");
    }

    if (neuron_recorder_record(NULL, &net, 0) ||
        neuron_recorder_record(&recorder, NULL, 0) ||
        neuron_recorder_record(&recorder, &net, -1))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("invalid recorder record call was accepted");
    }

    neuron_recorder_close(&recorder);
    neuron_recorder_close(&recorder);
    neuron_recorder_close(NULL);

    if (recorder.file != NULL || recorder.neuron_id != -1)
    {
        network_destroy(&net);
        return fail("recorder close did not reset fields");
    }

    if (neuron_recorder_write_header(&recorder))
    {
        network_destroy(&net);
        return fail("header write on closed recorder was accepted");
    }

    if (neuron_recorder_record(&recorder, &net, 0))
    {
        network_destroy(&net);
        return fail("record on closed recorder was accepted");
    }

    if (!neuron_recorder_open(&recorder, "test_neuron_recorder_invalid.csv", 5))
    {
        network_destroy(&net);
        return fail("recorder open for future neuron failed");
    }

    if (neuron_recorder_record(&recorder, &net, 0))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder accepted neuron out of range");
    }

    neuron_recorder_close(&recorder);
    remove("test_neuron_recorder_invalid.csv");

    network_destroy(&net);

    file = fopen(filename, "r");

    if (file == NULL)
        return fail("recorder output file was not created");

    if (fgets(header, sizeof(header), file) == NULL ||
        fgets(row, sizeof(row), file) == NULL)
    {
        fclose(file);
        return fail("recorder output file is incomplete");
    }

    fclose(file);
    remove(filename);

    if (strcmp(
            header,
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n") != 0)
    {
        return fail("recorder header is wrong");
    }

    if (strcmp(row, "0,-64.90,0,20.00,0.00\n") != 0)
        return fail("recorder row is wrong");

    return 1;
}

static int check_recorder_used_syn_current(void)
{
    const char *filename = "test_recorder_used_syn.csv";
    const char *missing_filename = "test_recorder_missing_used_syn.csv";
    char header[128];
    char row[128];
    FILE *file;
    Network net;
    NeuronRecorder recorder;
    double *saved_used_syn_current;

    remove(filename);
    remove(missing_filename);

    if (!network_init(&net, 1))
        return fail("network_init failed for used_syn_current recorder test");

    if (!same_double(net.used_syn_current[0], 0.0))
    {
        network_destroy(&net);
        return fail("used_syn_current did not start at 0");
    }

    net.syn_current[0] = 50.0;
    network_update(&net);

    if (!same_double(net.used_syn_current[0], 50.0))
    {
        network_destroy(&net);
        return fail("used_syn_current did not capture current used by LIF");
    }

    if (!same_double(net.syn_current[0], 50.0 * SYN_DECAY))
    {
        network_destroy(&net);
        return fail("syn_current did not decay after update");
    }

    if (!neuron_recorder_open(&recorder, filename, 0))
    {
        network_destroy(&net);
        return fail("recorder open failed for used_syn_current test");
    }

    if (!neuron_recorder_write_header(&recorder))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder header failed for used_syn_current test");
    }

    if (!neuron_recorder_record(&recorder, &net, 0))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder row failed for used_syn_current test");
    }

    neuron_recorder_close(&recorder);

    saved_used_syn_current = net.used_syn_current;
    net.used_syn_current = NULL;

    if (!neuron_recorder_open(&recorder, missing_filename, 0))
    {
        net.used_syn_current = saved_used_syn_current;
        network_destroy(&net);
        return fail("recorder open failed for missing used_syn_current test");
    }

    if (neuron_recorder_record(&recorder, &net, 0))
    {
        neuron_recorder_close(&recorder);
        net.used_syn_current = saved_used_syn_current;
        network_destroy(&net);
        return fail("recorder accepted network without used_syn_current");
    }

    neuron_recorder_close(&recorder);
    remove(missing_filename);

    net.used_syn_current = saved_used_syn_current;
    network_destroy(&net);

    file = fopen(filename, "r");

    if (file == NULL)
        return fail("used_syn_current recorder output was not created");

    if (fgets(header, sizeof(header), file) == NULL ||
        fgets(row, sizeof(row), file) == NULL)
    {
        fclose(file);
        return fail("used_syn_current recorder output is incomplete");
    }

    fclose(file);
    remove(filename);

    if (strcmp(
            header,
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n") != 0)
    {
        return fail("used_syn_current recorder header is wrong");
    }

    if (strcmp(row, "0,-64.75,0,0.00,50.00\n") != 0)
        return fail("recorder did not write used synaptic current");

    return 1;
}

static int check_population_recorder_api(void)
{
    const char *filename = "test_population_metrics.csv";
    char header[128];
    char row[128];
    char extra[128];
    FILE *file;
    Network net;
    PopulationRecorder recorder;
    PopulationRecorder invalid_recorder;
    LIFNeuron *saved_neurons;
    int *saved_spikes;
    double *saved_used_syn_current;
    double old_v0;
    double old_v1;
    double old_used0;
    double old_used1;
    int old_spike0;
    int old_spike1;
    NeuronType old_type0;
    NeuronType old_type1;

    remove(filename);

    if (population_recorder_open(NULL, filename))
        return fail("NULL population recorder open was accepted");

    invalid_recorder.file = (FILE *)1;

    if (population_recorder_open(&invalid_recorder, NULL))
        return fail("NULL population recorder filename was accepted");

    if (invalid_recorder.file != NULL)
        return fail("failed NULL population open did not reset recorder");

    invalid_recorder.file = (FILE *)1;

    if (population_recorder_open(&invalid_recorder, "."))
        return fail("invalid population recorder file was accepted");

    if (invalid_recorder.file != NULL)
        return fail("failed invalid population open did not reset recorder");

    population_recorder_close(NULL);

    if (!network_init(&net, 2))
        return fail("network_init failed for population recorder");

    if (!population_recorder_open(&recorder, filename))
    {
        network_destroy(&net);
        return fail("valid population recorder open failed");
    }

    if (!population_recorder_write_header(&recorder))
    {
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("valid population recorder header failed");
    }

    net.neurons[0].type = NEURON_EXCITATORY;
    net.neurons[0].V = -60.0;
    net.spikes[0] = 1;
    net.used_syn_current[0] = 10.0;

    net.neurons[1].type = NEURON_INHIBITORY;
    net.neurons[1].V = -70.0;
    net.spikes[1] = 0;
    net.used_syn_current[1] = -4.0;

    old_v0 = net.neurons[0].V;
    old_v1 = net.neurons[1].V;
    old_spike0 = net.spikes[0];
    old_spike1 = net.spikes[1];
    old_used0 = net.used_syn_current[0];
    old_used1 = net.used_syn_current[1];
    old_type0 = net.neurons[0].type;
    old_type1 = net.neurons[1].type;

    if (!population_recorder_record(&recorder, &net, 5))
    {
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("valid population recorder row failed");
    }

    if (!same_double(net.neurons[0].V, old_v0) ||
        !same_double(net.neurons[1].V, old_v1) ||
        net.spikes[0] != old_spike0 ||
        net.spikes[1] != old_spike1 ||
        !same_double(net.used_syn_current[0], old_used0) ||
        !same_double(net.used_syn_current[1], old_used1) ||
        net.neurons[0].type != old_type0 ||
        net.neurons[1].type != old_type1)
    {
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder changed network state");
    }

    if (population_recorder_record(NULL, &net, 5) ||
        population_recorder_record(&recorder, NULL, 5) ||
        population_recorder_record(&recorder, &net, -1))
    {
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("invalid population record call was accepted");
    }

    int saved_size = net.size;
    net.size = 0;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.size = saved_size;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted empty network");
    }

    net.size = saved_size;

    saved_neurons = net.neurons;
    net.neurons = NULL;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.neurons = saved_neurons;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted NULL neurons");
    }

    net.neurons = saved_neurons;

    saved_spikes = net.spikes;
    net.spikes = NULL;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.spikes = saved_spikes;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted NULL spikes");
    }

    net.spikes = saved_spikes;

    saved_used_syn_current = net.used_syn_current;
    net.used_syn_current = NULL;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.used_syn_current = saved_used_syn_current;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted NULL used_syn_current");
    }

    net.used_syn_current = saved_used_syn_current;

    net.spikes[1] = 2;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.spikes[1] = old_spike1;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted invalid spike");
    }

    net.spikes[1] = old_spike1;

    net.neurons[0].type = (NeuronType)99;

    if (population_recorder_record(&recorder, &net, 5))
    {
        net.neurons[0].type = old_type0;
        population_recorder_close(&recorder);
        network_destroy(&net);
        return fail("population recorder accepted invalid neuron type");
    }

    net.neurons[0].type = old_type0;

    population_recorder_close(&recorder);
    population_recorder_close(&recorder);

    if (recorder.file != NULL)
    {
        network_destroy(&net);
        return fail("population recorder close did not reset file");
    }

    if (population_recorder_write_header(&recorder) ||
        population_recorder_record(&recorder, &net, 5))
    {
        network_destroy(&net);
        return fail("closed population recorder accepted operation");
    }

    network_destroy(&net);

    file = fopen(filename, "r");

    if (file == NULL)
        return fail("population recorder output file was not created");

    if (fgets(header, sizeof(header), file) == NULL ||
        fgets(row, sizeof(row), file) == NULL)
    {
        fclose(file);
        return fail("population recorder output is incomplete");
    }

    if (fgets(extra, sizeof(extra), file) != NULL)
    {
        fclose(file);
        remove(filename);
        return fail("population recorder wrote an unexpected extra row");
    }

    fclose(file);
    remove(filename);

    if (strcmp(
            header,
            "tempo,spikes_total,spikes_exc,spikes_inh,"
            "mean_potential,mean_syn_current\n") != 0)
    {
        return fail("population recorder header is wrong");
    }

    if (strcmp(row, "5,1,1,0,-65.00,3.00\n") != 0)
        return fail("population recorder row is wrong");

    return 1;
}

static int check_network_connect_api(void)
{
    Network net;

    if (!network_init(&net, 3))
        return fail("network_init failed for network_connect");

    if (!network_connect(&net, 0, 1, 2.5))
    {
        network_destroy(&net);
        return fail("valid connection was rejected");
    }

    if (net.connections[0].count != 1 ||
        net.connections[0].list == NULL ||
        net.connections[0].list[0].target != 1 ||
        !same_double(net.connections[0].list[0].weight, 2.5) ||
        net.connections[0].list[0].delay != 1)
    {
        network_destroy(&net);
        return fail("valid connection was not stored correctly");
    }

    if (!network_connect_delayed(&net, 0, 2, 4.0, 3))
    {
        network_destroy(&net);
        return fail("valid delayed connection was rejected");
    }

    if (net.connections[0].count != 2 ||
        net.connections[0].list[1].target != 2 ||
        !same_double(net.connections[0].list[1].weight, 4.0) ||
        net.connections[0].list[1].delay != 3)
    {
        network_destroy(&net);
        return fail("valid delayed connection was not stored correctly");
    }

    if (network_connect(&net, -1, 2, W))
    {
        network_destroy(&net);
        return fail("invalid source was accepted");
    }

    if (network_connect(&net, 0, 4, W))
    {
        network_destroy(&net);
        return fail("invalid target was accepted");
    }

    if (network_connect(&net, 1, 1, W))
    {
        network_destroy(&net);
        return fail("self-connection was accepted");
    }

    if (network_connect(&net, 0, 1, 99.0))
    {
        network_destroy(&net);
        return fail("duplicate connection was accepted");
    }

    if (network_connect_delayed(&net, 0, 1, 99.0, 2))
    {
        network_destroy(&net);
        return fail("duplicate delayed connection was accepted");
    }

    if (network_connect_delayed(&net, 1, 2, W, 0))
    {
        network_destroy(&net);
        return fail("zero delay was accepted");
    }

    if (network_connect_delayed(&net, 1, 2, W, -1))
    {
        network_destroy(&net);
        return fail("negative delay was accepted");
    }

    if (network_connect_delayed(
            &net,
            1,
            2,
            W,
            MAX_SYNAPTIC_DELAY + 1))
    {
        network_destroy(&net);
        return fail("too-large delay was accepted");
    }

    if (network_connect(&net, 1, 2, NAN))
    {
        network_destroy(&net);
        return fail("invalid weight was accepted");
    }

    if (network_connect(&net, 1, 2, INFINITY))
    {
        network_destroy(&net);
        return fail("infinite weight was accepted");
    }

    if (net.connections[0].count != 2 ||
        net.connections[0].list[0].target != 1 ||
        !same_double(net.connections[0].list[0].weight, 2.5) ||
        net.connections[0].list[0].delay != 1 ||
        net.connections[0].list[1].target != 2 ||
        !same_double(net.connections[0].list[1].weight, 4.0) ||
        net.connections[0].list[1].delay != 3)
    {
        network_destroy(&net);
        return fail("invalid connect call changed the original list");
    }

    network_destroy(&net);
    return 1;
}

static int check_synaptic_delay_timing(void)
{
    const char *filename = "test_recorder_delay.csv";
    char header[128];
    char row[128];
    FILE *file;
    Network net;
    NeuronRecorder recorder;

    remove(filename);

    if (!network_init(&net, 2))
        return fail("network_init failed for delay 1 timing");

    if (!network_connect(&net, 0, 1, 75.0))
    {
        network_destroy(&net);
        return fail("delay 1 connection setup failed");
    }

    if (!network_set_external_current(&net, 0, 4000.0))
    {
        network_destroy(&net);
        return fail("delay 1 input setup failed");
    }

    network_update(&net);

    if (!net.spikes[0] || !same_double(net.used_syn_current[1], 0.0))
    {
        network_destroy(&net);
        return fail("delay 1 source spike setup failed");
    }

    network_clear_external_currents(&net);
    network_update(&net);

    if (!same_double(net.used_syn_current[1], 75.0))
    {
        network_destroy(&net);
        return fail("delay 1 did not arrive on the next timestep");
    }

    network_destroy(&net);

    if (!network_init(&net, 2))
        return fail("network_init failed for delay 3 timing");

    if (!network_connect_delayed(&net, 0, 1, 120.0, 3))
    {
        network_destroy(&net);
        return fail("delay 3 connection setup failed");
    }

    if (!network_set_external_current(&net, 0, 4000.0))
    {
        network_destroy(&net);
        return fail("delay 3 input setup failed");
    }

    network_update(&net);

    if (!net.spikes[0] || !same_double(net.used_syn_current[1], 0.0))
    {
        network_destroy(&net);
        return fail("delay 3 timestep 0 was not empty");
    }

    network_clear_external_currents(&net);
    network_update(&net);

    if (!same_double(net.used_syn_current[1], 0.0))
    {
        network_destroy(&net);
        return fail("delay 3 arrived too early at timestep 1");
    }

    network_update(&net);

    if (!same_double(net.used_syn_current[1], 0.0))
    {
        network_destroy(&net);
        return fail("delay 3 arrived too early at timestep 2");
    }

    network_update(&net);

    if (!same_double(net.used_syn_current[1], 120.0))
    {
        network_destroy(&net);
        return fail("delay 3 did not arrive at timestep 3");
    }

    if (!neuron_recorder_open(&recorder, filename, 1))
    {
        network_destroy(&net);
        return fail("recorder open failed for delay test");
    }

    if (!neuron_recorder_write_header(&recorder))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder header failed for delay test");
    }

    if (!neuron_recorder_record(&recorder, &net, 3))
    {
        neuron_recorder_close(&recorder);
        network_destroy(&net);
        return fail("recorder row failed for delay test");
    }

    neuron_recorder_close(&recorder);
    network_destroy(&net);

    file = fopen(filename, "r");

    if (file == NULL)
        return fail("delay recorder output was not created");

    if (fgets(header, sizeof(header), file) == NULL ||
        fgets(row, sizeof(row), file) == NULL)
    {
        fclose(file);
        return fail("delay recorder output is incomplete");
    }

    fclose(file);
    remove(filename);

    if (strcmp(
            header,
            "tempo,V,spike,corrente_externa,corrente_sinaptica\n") != 0)
    {
        return fail("delay recorder header is wrong");
    }

    if (strcmp(row, "3,-64.40,0,0.00,120.00\n") != 0)
        return fail("delay recorder did not write current used at timestep");

    if (!network_init(&net, 2))
        return fail("network_init failed for negative delay timing");

    if (!network_connect_delayed(&net, 0, 1, -80.0, 3))
    {
        network_destroy(&net);
        return fail("negative delayed connection setup failed");
    }

    if (!network_set_external_current(&net, 0, 4000.0))
    {
        network_destroy(&net);
        return fail("negative delay input setup failed");
    }

    network_update(&net);
    network_clear_external_currents(&net);
    network_update(&net);
    network_update(&net);
    network_update(&net);

    if (!same_double(net.used_syn_current[1], -80.0))
    {
        network_destroy(&net);
        return fail("negative delayed current did not arrive");
    }

    if (net.neurons[1].V >= V_REST)
    {
        network_destroy(&net);
        return fail("negative delayed current did not hyperpolarize target");
    }

    network_destroy(&net);
    return 1;
}

static int check_clear_connections(void)
{
    Network net;

    if (!network_init(&net, 4))
        return fail("network_init failed for clear connections");

    if (!network_connect(&net, 0, 1, W) ||
        !network_connect(&net, 0, 2, W) ||
        !network_connect(&net, 1, 2, W))
    {
        network_destroy(&net);
        return fail("setup failed for clear connections");
    }

    network_clear_connections(&net);
    network_clear_connections(&net);
    network_clear_connections(NULL);

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].list != NULL ||
            net.connections[i].count != 0)
        {
            network_destroy(&net);
            return fail("network_clear_connections did not clear a list");
        }
    }

    network_destroy(&net);
    return 1;
}

static int check_topology_replacement(void)
{
    Network net;

    if (!network_init(&net, 5))
        return fail("network_init failed for topology replacement");

    if (!topology_all_to_all(&net))
    {
        network_destroy(&net);
        return fail("all-to-all setup failed for topology replacement");
    }

    if (!network_set_neuron_type(&net, 0, NEURON_INHIBITORY))
    {
        network_destroy(&net);
        return fail("manual inhibitory setup failed for topology replacement");
    }

    if (!topology_ring(&net))
    {
        network_destroy(&net);
        return fail("ring setup failed for topology replacement");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("old topology did not restore excitatory types");
    }

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].count != 1 ||
            net.connections[i].list[0].target != (i + 1) % net.size)
        {
            network_destroy(&net);
            return fail("topology replacement left old connections");
        }
    }

    if (!topology_chain(&net))
    {
        network_destroy(&net);
        return fail("chain setup failed for topology replacement");
    }

    for (int i = 0; i < net.size; i++)
    {
        int expected_count = (i == net.size - 1) ? 0 : 1;

        if (net.connections[i].count != expected_count)
        {
            network_destroy(&net);
            return fail("second topology replacement left old counts");
        }
    }

    network_destroy(&net);
    return 1;
}

static int check_chain(int size)
{
    Network net;

    if (!network_init(&net, size))
        return fail("network_init failed for chain");

    if (!topology_chain(&net))
    {
        network_destroy(&net);
        return fail("topology_chain returned failure");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("chain did not keep excitatory neuron types");
    }

    if (!all_connections_have_delay(&net, 1))
    {
        network_destroy(&net);
        return fail("chain did not keep delay 1");
    }

    if (!apply_test_current(&net))
    {
        network_destroy(&net);
        return 0;
    }

    for (int i = 0; i < net.size; i++)
    {
        int expected_count = (i == net.size - 1) ? 0 : 1;

        if (net.connections[i].count != expected_count)
        {
            network_destroy(&net);
            return fail("chain connection count is wrong");
        }

        if (expected_count == 1 &&
            net.connections[i].list[0].target != i + 1)
        {
            network_destroy(&net);
            return fail("chain target is wrong");
        }
    }

    if (!check_runtime_state(&net, 400))
    {
        network_destroy(&net);
        return 0;
    }

    network_destroy(&net);
    return 1;
}

static int check_ring(int size)
{
    Network net;

    if (!network_init(&net, size))
        return fail("network_init failed for ring");

    if (!topology_ring(&net))
    {
        network_destroy(&net);
        return fail("topology_ring returned failure");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("ring did not keep excitatory neuron types");
    }

    if (!all_connections_have_delay(&net, 1))
    {
        network_destroy(&net);
        return fail("ring did not keep delay 1");
    }

    if (!apply_test_current(&net))
    {
        network_destroy(&net);
        return 0;
    }

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].count != 1)
        {
            network_destroy(&net);
            return fail("ring connection count is wrong");
        }

        if (net.connections[i].list[0].target != (i + 1) % net.size)
        {
            network_destroy(&net);
            return fail("ring target is wrong");
        }
    }

    if (!check_runtime_state(&net, 400))
    {
        network_destroy(&net);
        return 0;
    }

    network_destroy(&net);
    return 1;
}

static int check_all_to_all(void)
{
    Network net;

    if (!network_init(&net, 5))
        return fail("network_init failed for all-to-all");

    if (!topology_all_to_all(&net))
    {
        network_destroy(&net);
        return fail("topology_all_to_all returned failure");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("all-to-all did not keep excitatory neuron types");
    }

    if (!all_connections_have_delay(&net, 1))
    {
        network_destroy(&net);
        return fail("all-to-all did not keep delay 1");
    }

    if (!apply_test_current(&net))
    {
        network_destroy(&net);
        return 0;
    }

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].count != net.size - 1)
        {
            network_destroy(&net);
            return fail("all-to-all connection count is wrong");
        }

        if (!check_no_duplicate_targets(&net.connections[i]))
        {
            network_destroy(&net);
            return fail("all-to-all has duplicate targets");
        }

        for (int target = 0; target < net.size; target++)
        {
            if (target == i)
                continue;

            if (!has_target(&net.connections[i], target))
            {
                network_destroy(&net);
                return fail("all-to-all target is missing");
            }
        }
    }

    if (!check_runtime_state(&net, 400))
    {
        network_destroy(&net);
        return 0;
    }

    network_destroy(&net);
    return 1;
}

static int check_random(void)
{
    Network net;

    if (!network_init(&net, 10))
        return fail("network_init failed for random");

    srand(RANDOM_SEED);
    if (!topology_random(&net, 0.20))
    {
        network_destroy(&net);
        return fail("topology_random returned failure");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("random did not keep excitatory neuron types");
    }

    if (!all_connections_have_delay(&net, 1))
    {
        network_destroy(&net);
        return fail("random did not keep delay 1");
    }

    if (!apply_test_current(&net))
    {
        network_destroy(&net);
        return 0;
    }

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].count < 0 ||
            net.connections[i].count > net.size - 1)
        {
            network_destroy(&net);
            return fail("random connection count is out of range");
        }

        if (net.connections[i].count == 0 &&
            net.connections[i].list != NULL)
        {
            network_destroy(&net);
            return fail("random empty list is not NULL");
        }

        if (net.connections[i].count > 0 &&
            net.connections[i].list == NULL)
        {
            network_destroy(&net);
            return fail("random non-empty list is NULL");
        }

        if (!check_no_duplicate_targets(&net.connections[i]))
        {
            network_destroy(&net);
            return fail("random has duplicate targets");
        }

        for (int j = 0; j < net.connections[i].count; j++)
        {
            int target = net.connections[i].list[j].target;

            if (target < 0 || target >= net.size)
            {
                network_destroy(&net);
                return fail("random target is out of range");
            }

            if (target == i)
            {
                network_destroy(&net);
                return fail("random has self-connection");
            }
        }
    }

    if (!check_runtime_state(&net, 400))
    {
        network_destroy(&net);
        return 0;
    }

    network_destroy(&net);
    return 1;
}

static int check_random_balanced(void)
{
    Network net;
    Network first;
    Network second;
    int inhibitory_count;
    int expected_inhibitory;

    if (!network_init(&net, 10))
        return fail("network_init failed for random balanced");

    if (topology_random_balanced(NULL, 0.20, 0.20))
    {
        network_destroy(&net);
        return fail("balanced topology accepted NULL network");
    }

    if (topology_random_balanced(&net, -0.01, 0.20) ||
        topology_random_balanced(&net, 1.01, 0.20) ||
        topology_random_balanced(&net, NAN, 0.20))
    {
        network_destroy(&net);
        return fail("balanced topology accepted invalid probability");
    }

    if (topology_random_balanced(&net, 0.20, -0.01) ||
        topology_random_balanced(&net, 0.20, 1.01) ||
        topology_random_balanced(&net, 0.20, INFINITY))
    {
        network_destroy(&net);
        return fail("balanced topology accepted invalid inhibitory fraction");
    }

    if (!topology_random_balanced(&net, 1.0, 0.30))
    {
        network_destroy(&net);
        return fail("topology_random_balanced returned failure");
    }

    if (!all_connections_have_delay(&net, 1))
    {
        network_destroy(&net);
        return fail("balanced topology did not keep delay 1");
    }

    inhibitory_count = count_neurons_with_type(&net, NEURON_INHIBITORY);
    expected_inhibitory = (int)(0.30 * (double)net.size + 0.5);

    if (abs(inhibitory_count - expected_inhibitory) > 1)
    {
        network_destroy(&net);
        return fail("balanced topology inhibitory count is far from expected");
    }

    for (int i = 0; i < net.size; i++)
    {
        if (net.connections[i].count != net.size - 1)
        {
            network_destroy(&net);
            return fail("balanced probability 1 did not create all valid targets");
        }

        if (!check_no_duplicate_targets(&net.connections[i]))
        {
            network_destroy(&net);
            return fail("balanced topology has duplicate targets");
        }

        for (int j = 0; j < net.connections[i].count; j++)
        {
            int target = net.connections[i].list[j].target;
            double weight = net.connections[i].list[j].weight;

            if (target < 0 || target >= net.size)
            {
                network_destroy(&net);
                return fail("balanced target is out of range");
            }

            if (target == i)
            {
                network_destroy(&net);
                return fail("balanced topology has self-connection");
            }

            if (net.neurons[i].type == NEURON_INHIBITORY)
            {
                if (weight >= 0.0 || !same_double(weight, W_INH))
                {
                    network_destroy(&net);
                    return fail("inhibitory neuron produced non-inhibitory weight");
                }
            }
            else
            {
                if (weight <= 0.0 || !same_double(weight, W_EXC))
                {
                    network_destroy(&net);
                    return fail("excitatory neuron produced non-excitatory weight");
                }
            }
        }
    }

    if (!topology_chain(&net))
    {
        network_destroy(&net);
        return fail("old topology failed after balanced topology");
    }

    if (!all_neurons_have_type(&net, NEURON_EXCITATORY))
    {
        network_destroy(&net);
        return fail("old topology did not restore EXC after balanced topology");
    }

    network_destroy(&net);

    if (!network_init(&first, 12))
        return fail("first deterministic balanced network init failed");

    if (!network_init(&second, 12))
    {
        network_destroy(&first);
        return fail("second deterministic balanced network init failed");
    }

    if (!topology_random_balanced(&first, 0.35, 0.25) ||
        !topology_random_balanced(&second, 0.35, 0.25))
    {
        network_destroy(&first);
        network_destroy(&second);
        return fail("deterministic balanced topology setup failed");
    }

    if (!same_topology_state(&first, &second))
    {
        network_destroy(&first);
        network_destroy(&second);
        return fail("balanced topology was not deterministic");
    }

    network_destroy(&first);
    network_destroy(&second);
    return 1;
}

static int check_inhibitory_effect(void)
{
    Network net;

    if (!network_init(&net, 2))
        return fail("network_init failed for inhibitory effect");

    if (!network_set_neuron_type(&net, 0, NEURON_INHIBITORY))
    {
        network_destroy(&net);
        return fail("failed to set inhibitory source neuron");
    }

    if (!network_connect(&net, 0, 1, W_INH))
    {
        network_destroy(&net);
        return fail("negative inhibitory connection was rejected");
    }

    if (!network_set_external_current(&net, 0, 4000.0))
    {
        network_destroy(&net);
        return fail("failed to set strong input for inhibitory effect");
    }

    network_update(&net);

    if (!net.spikes[0])
    {
        network_destroy(&net);
        return fail("inhibitory source did not spike");
    }

    network_clear_external_currents(&net);
    network_update(&net);

    if (net.used_syn_current[1] >= 0.0)
    {
        network_destroy(&net);
        return fail("inhibitory synaptic current was not negative");
    }

    if (net.neurons[1].V >= V_REST)
    {
        network_destroy(&net);
        return fail("inhibitory current did not hyperpolarize target neuron");
    }

    network_destroy(&net);
    return 1;
}

int main(void)
{
    if (!check_initial_state(10))
        return 1;

    if (!check_network_connect_api())
        return 1;

    if (!check_synaptic_delay_timing())
        return 1;

    if (!check_neuron_type_api())
        return 1;

    if (!check_external_current_api())
        return 1;

    if (!check_pulse_stimulus_api())
        return 1;

    if (!check_stimulus_schedule_api())
        return 1;

    if (!check_neuron_recorder_api())
        return 1;

    if (!check_recorder_used_syn_current())
        return 1;

    if (!check_population_recorder_api())
        return 1;

    if (!check_clear_connections())
        return 1;

    if (!check_topology_replacement())
        return 1;

    if (!check_chain(2) || !check_chain(5))
        return 1;

    if (!check_ring(2) || !check_ring(5))
        return 1;

    if (!check_all_to_all())
        return 1;

    if (!check_random())
        return 1;

    if (!check_random_balanced())
        return 1;

    if (!check_inhibitory_effect())
        return 1;

    printf("Topology, stimulus, and recorder validation OK\n");
    return 0;
}

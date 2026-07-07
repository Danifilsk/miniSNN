#include <stdio.h>

#include "minisnn.h"

#define TEST_WEIGHT 200.0
#define TEST_INPUT 20.0

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 0;
}

static int same_double(double a, double b)
{
    double diff = a - b;
    return diff > -1e-9 && diff < 1e-9;
}

static double make_nan(void)
{
    volatile double zero = 0.0;
    return zero / zero;
}

static int config_looks_valid(const MiniSNNConfig *config)
{
    return config != NULL &&
           config->neuron_count > 0 &&
           config->dt > 0.0 &&
           config->tau > 0.0 &&
           config->resistance > 0.0 &&
           config->v_rest < config->v_threshold &&
           config->v_reset < config->v_threshold &&
           config->synaptic_decay >= 0.0 &&
           config->synaptic_decay <= 1.0 &&
           config->max_synaptic_delay >= 1;
}

static int check_default_config_and_invalid_configs(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNNConfig invalid;

    if (!config_looks_valid(&config))
        return fail("default config did not look valid");

    if (minisnn_create_with_config(NULL) != NULL)
        return fail("create_with_config accepted NULL config");

    invalid = config;
    invalid.neuron_count = 0;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted zero neurons");

    invalid = config;
    invalid.dt = 0.0;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted zero dt");

    invalid = config;
    invalid.tau = 0.0;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted zero tau");

    invalid = config;
    invalid.resistance = 0.0;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted zero resistance");

    invalid = config;
    invalid.v_rest = invalid.v_threshold;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted v_rest >= threshold");

    invalid = config;
    invalid.v_reset = invalid.v_threshold;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted v_reset >= threshold");

    invalid = config;
    invalid.synaptic_decay = -0.01;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted negative synaptic decay");

    invalid = config;
    invalid.synaptic_decay = 1.01;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted synaptic decay greater than 1");

    invalid = config;
    invalid.max_synaptic_delay = 0;
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted zero max delay");

    invalid = config;
    invalid.dt = make_nan();
    if (minisnn_create_with_config(&invalid) != NULL)
        return fail("create_with_config accepted NaN");

    return 1;
}

static int check_create_and_invalid_calls(void)
{
    MiniSNN *null_net = NULL;

    if (minisnn_create(0) != NULL)
        return fail("minisnn_create accepted zero neurons");

    if (minisnn_step(NULL) != -1)
        return fail("minisnn_step accepted NULL");

    minisnn_destroy(NULL);
    minisnn_destroy(&null_net);
    minisnn_clear_inputs(NULL);

    if (minisnn_set_neuron_type(NULL, 0, MINISNN_NEURON_EXCITATORY))
        return fail("set_neuron_type accepted NULL");

    if (minisnn_connect(NULL, 0, 1, TEST_WEIGHT))
        return fail("connect accepted NULL");

    if (minisnn_connect_delayed(NULL, 0, 1, TEST_WEIGHT, 1))
        return fail("connect_delayed accepted NULL");

    if (minisnn_set_input(NULL, 0, TEST_INPUT))
        return fail("set_input accepted NULL");

    if (minisnn_add_input(NULL, 0, TEST_INPUT))
        return fail("add_input accepted NULL");

    return 1;
}

static int check_basic_network_api(void)
{
    MiniSNN *net = minisnn_create(3);
    int spike = -1;
    int spike_count;
    double voltage = 0.0;
    double syn_current = 0.0;

    if (net == NULL)
        return fail("minisnn_create failed for 3 neurons");

    if (minisnn_neuron_count(net) != 3)
    {
        minisnn_destroy(&net);
        return fail("neuron count was not 3");
    }

    if (minisnn_current_step(net) != 0)
    {
        minisnn_destroy(&net);
        return fail("current step did not start at 0");
    }

    if (!minisnn_set_neuron_type(net, 1, MINISNN_NEURON_INHIBITORY))
    {
        minisnn_destroy(&net);
        return fail("valid inhibitory neuron type was rejected");
    }

    if (minisnn_set_neuron_type(net, -1, MINISNN_NEURON_EXCITATORY) ||
        minisnn_set_neuron_type(net, 3, MINISNN_NEURON_EXCITATORY) ||
        minisnn_set_neuron_type(net, 0, (MiniSNNNeuronType)99))
    {
        minisnn_destroy(&net);
        return fail("invalid neuron type call was accepted");
    }

    if (!minisnn_connect(net, 0, 1, TEST_WEIGHT))
    {
        minisnn_destroy(&net);
        return fail("valid normal connection was rejected");
    }

    if (!minisnn_connect_delayed(net, 0, 2, TEST_WEIGHT, 2))
    {
        minisnn_destroy(&net);
        return fail("valid delayed connection was rejected");
    }

    if (minisnn_connect(net, -1, 1, TEST_WEIGHT) ||
        minisnn_connect(net, 0, 3, TEST_WEIGHT) ||
        minisnn_connect(net, 1, 1, TEST_WEIGHT) ||
        minisnn_connect_delayed(net, 0, 1, TEST_WEIGHT, 1))
    {
        minisnn_destroy(&net);
        return fail("invalid or duplicate connection was accepted");
    }

    if (!minisnn_set_input(net, 0, TEST_INPUT))
    {
        minisnn_destroy(&net);
        return fail("valid input was rejected");
    }

    if (!minisnn_add_input(net, 0, 1.0))
    {
        minisnn_destroy(&net);
        return fail("valid input addition was rejected");
    }

    if (minisnn_set_input(net, -1, TEST_INPUT) ||
        minisnn_set_input(net, 3, TEST_INPUT) ||
        minisnn_add_input(net, -1, TEST_INPUT) ||
        minisnn_add_input(net, 3, TEST_INPUT))
    {
        minisnn_destroy(&net);
        return fail("invalid input call was accepted");
    }

    spike_count = minisnn_step(net);

    if (spike_count < 0)
    {
        minisnn_destroy(&net);
        return fail("minisnn_step failed on valid network");
    }

    if (minisnn_current_step(net) != 1)
    {
        minisnn_destroy(&net);
        return fail("minisnn_step did not advance time");
    }

    if (!minisnn_get_spike(net, 0, &spike))
    {
        minisnn_destroy(&net);
        return fail("valid spike getter failed");
    }

    if (spike != 0 && spike != 1)
    {
        minisnn_destroy(&net);
        return fail("spike getter returned invalid value");
    }

    if (!minisnn_get_voltage(net, 0, &voltage))
    {
        minisnn_destroy(&net);
        return fail("valid voltage getter failed");
    }

    if (voltage < -1000000.0 || voltage > 1000000.0)
    {
        minisnn_destroy(&net);
        return fail("voltage getter returned absurd value");
    }

    if (!minisnn_get_synaptic_current(net, 0, &syn_current))
    {
        minisnn_destroy(&net);
        return fail("valid synaptic current getter failed");
    }

    if (syn_current < -1000000.0 || syn_current > 1000000.0)
    {
        minisnn_destroy(&net);
        return fail("synaptic current getter returned absurd value");
    }

    if (minisnn_get_spike(net, -1, &spike) ||
        minisnn_get_spike(net, 3, &spike) ||
        minisnn_get_spike(net, 0, NULL) ||
        minisnn_get_voltage(net, -1, &voltage) ||
        minisnn_get_voltage(net, 3, &voltage) ||
        minisnn_get_voltage(net, 0, NULL) ||
        minisnn_get_synaptic_current(net, -1, &syn_current) ||
        minisnn_get_synaptic_current(net, 3, &syn_current) ||
        minisnn_get_synaptic_current(net, 0, NULL))
    {
        minisnn_destroy(&net);
        return fail("invalid getter call was accepted");
    }

    minisnn_clear_inputs(net);
    minisnn_destroy(&net);

    if (net != NULL)
        return fail("minisnn_destroy did not set pointer to NULL");

    minisnn_destroy(&net);
    return 1;
}

static int check_custom_config_delay_limit(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *net;

    config.neuron_count = 3;
    config.max_synaptic_delay = 3;

    net = minisnn_create_with_config(&config);

    if (net == NULL)
        return fail("custom valid config was rejected");

    if (!minisnn_connect_delayed(net, 0, 1, TEST_WEIGHT, 3))
    {
        minisnn_destroy(&net);
        return fail("delay 3 was rejected with max delay 3");
    }

    if (minisnn_connect_delayed(net, 1, 2, TEST_WEIGHT, 4))
    {
        minisnn_destroy(&net);
        return fail("delay 4 was accepted with max delay 3");
    }

    minisnn_destroy(&net);
    return 1;
}

static int check_independent_configs(void)
{
    MiniSNNConfig slow_config = minisnn_default_config();
    MiniSNNConfig fast_config = minisnn_default_config();
    MiniSNN *slow;
    MiniSNN *fast;
    double slow_voltage = 0.0;
    double fast_voltage = 0.0;

    slow_config.neuron_count = 1;
    slow_config.dt = 0.1;

    fast_config.neuron_count = 1;
    fast_config.dt = 1.0;

    slow = minisnn_create_with_config(&slow_config);
    fast = minisnn_create_with_config(&fast_config);

    if (slow == NULL || fast == NULL)
    {
        minisnn_destroy(&slow);
        minisnn_destroy(&fast);
        return fail("independent custom networks could not be created");
    }

    if (!minisnn_set_input(slow, 0, TEST_INPUT) ||
        !minisnn_set_input(fast, 0, TEST_INPUT) ||
        minisnn_step(slow) < 0 ||
        minisnn_step(fast) < 0 ||
        !minisnn_get_voltage(slow, 0, &slow_voltage) ||
        !minisnn_get_voltage(fast, 0, &fast_voltage))
    {
        minisnn_destroy(&slow);
        minisnn_destroy(&fast);
        return fail("independent custom networks failed to run");
    }

    if (same_double(slow_voltage, fast_voltage))
    {
        minisnn_destroy(&slow);
        minisnn_destroy(&fast);
        return fail("different dt values produced identical voltages");
    }

    minisnn_destroy(&slow);
    minisnn_destroy(&fast);
    return 1;
}

static int check_config_copy_on_create(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *net;
    double voltage = 0.0;

    config.neuron_count = 1;
    config.dt = 0.1;

    net = minisnn_create_with_config(&config);

    if (net == NULL)
        return fail("copy-on-create network could not be created");

    config.dt = 1.0;
    config.v_rest = -10.0;

    if (!minisnn_set_input(net, 0, TEST_INPUT) ||
        minisnn_step(net) < 0 ||
        !minisnn_get_voltage(net, 0, &voltage))
    {
        minisnn_destroy(&net);
        return fail("copy-on-create network failed to run");
    }

    if (!same_double(voltage, -64.9))
    {
        minisnn_destroy(&net);
        return fail("changing config after create changed existing network");
    }

    minisnn_destroy(&net);
    return 1;
}

static int check_recreate_after_destroy(void)
{
    MiniSNN *net = minisnn_create(2);

    if (net == NULL)
        return fail("minisnn_create failed after previous destroy");

    if (minisnn_neuron_count(net) != 2)
    {
        minisnn_destroy(&net);
        return fail("second network had wrong size");
    }

    if (minisnn_step(net) < 0)
    {
        minisnn_destroy(&net);
        return fail("second network failed to step");
    }

    minisnn_destroy(&net);
    minisnn_destroy(&net);
    return 1;
}

int main(void)
{
    if (!check_default_config_and_invalid_configs())
        return 1;

    if (!check_create_and_invalid_calls())
        return 1;

    if (!check_basic_network_api())
        return 1;

    if (!check_custom_config_delay_limit())
        return 1;

    if (!check_independent_configs())
        return 1;

    if (!check_config_copy_on_create())
        return 1;

    if (!check_recreate_after_destroy())
        return 1;

    printf("MiniSNN public API validation OK\n");
    return 0;
}

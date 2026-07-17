#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"
#include "neuron.h"
#include "neuron_model.h"

static int check_public_effective_dt(
    MiniSNNNeuronModel model,
    double effective_dt,
    double changed_dt,
    double input_current)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *first = NULL;
    MiniSNN *nested_changed = NULL;
    MiniSNN *dt_changed = NULL;
    unsigned long long signature;
    unsigned long long changed_signature;
    double first_voltage;
    double nested_voltage;
    double changed_voltage;
    int ok = 0;

    config.neuron_count = 1;
    config.neuron_model = model;
    config.dt = effective_dt;
    if (model == MINISNN_NEURON_MODEL_ADEX)
        config.adex.dt = changed_dt;
    else if (model == MINISNN_NEURON_MODEL_HODGKIN_HUXLEY)
        config.hodgkin_huxley.dt = changed_dt;
    else
        return 0;

    signature = minisnn_config_neuron_model_signature(&config);
    first = minisnn_create_with_config(&config);
    if (signature == 0ULL || first == NULL ||
        minisnn_neuron_model_config_signature(first) != signature)
        goto done;

    if (model == MINISNN_NEURON_MODEL_ADEX)
        config.adex.dt = effective_dt * 7.0;
    else
        config.hodgkin_huxley.dt = effective_dt * 7.0;
    if (minisnn_config_neuron_model_signature(&config) != signature)
        goto done;
    nested_changed = minisnn_create_with_config(&config);
    if (nested_changed == NULL ||
        minisnn_neuron_model_config_signature(nested_changed) != signature)
        goto done;

    config.dt = changed_dt;
    changed_signature = minisnn_config_neuron_model_signature(&config);
    dt_changed = minisnn_create_with_config(&config);
    if (changed_signature == 0ULL || changed_signature == signature ||
        dt_changed == NULL ||
        minisnn_neuron_model_config_signature(dt_changed) != changed_signature)
        goto done;

    if (!minisnn_set_input(first, 0, input_current) ||
        !minisnn_set_input(nested_changed, 0, input_current) ||
        !minisnn_set_input(dt_changed, 0, input_current) ||
        minisnn_step(first) < 0 || minisnn_step(nested_changed) < 0 ||
        minisnn_step(dt_changed) < 0 ||
        !minisnn_get_voltage(first, 0, &first_voltage) ||
        !minisnn_get_voltage(nested_changed, 0, &nested_voltage) ||
        !minisnn_get_voltage(dt_changed, 0, &changed_voltage) ||
        fabs(first_voltage - nested_voltage) > 1e-12 ||
        fabs(first_voltage - changed_voltage) <= 1e-12)
        goto done;

    config.dt = 0.0;
    if (minisnn_config_neuron_model_signature(&config) != 0ULL ||
        minisnn_create_with_config(&config) != NULL)
        goto done;

    ok = 1;
done:
    minisnn_destroy(&first);
    minisnn_destroy(&nested_changed);
    minisnn_destroy(&dt_changed);
    return ok;
}

int main(void)
{
    LIFParameters parameters;
    LIFNeuron direct;
    LIFNeuron dispatched;
    NeuronModelConfig model_config;
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *first = NULL;
    MiniSNN *second = NULL;
    LIFNeuron api_reference;
    NeuronModelConfig changed_config;
    unsigned long long signature;
    MiniSNNNeuronModel parsed_model;
    MiniSNNConfig signature_config;
    unsigned long long public_signature;
    uint64_t topology_signatures[3];

    lif_parameters_default(&parameters);
    neuron_model_config_lif(&model_config, &parameters);
    if (config.neuron_model != MINISNN_NEURON_MODEL_LIF ||
        !neuron_model_is_valid(MINISNN_NEURON_MODEL_LIF) ||
        !neuron_model_validate_config(&model_config) ||
        !neuron_model_supports_adaptive_threshold(MINISNN_NEURON_MODEL_LIF))
        return 1;
    if (neuron_model_is_valid((MiniSNNNeuronModel)99))
        return 1;
    if (!neuron_model_from_name("LiF", &parsed_model) ||
        parsed_model != MINISNN_NEURON_MODEL_LIF ||
        !neuron_model_from_name("ADEX", &parsed_model) ||
        parsed_model != MINISNN_NEURON_MODEL_ADEX ||
        !neuron_model_from_name("hodgkin-huxley", &parsed_model) ||
        parsed_model != MINISNN_NEURON_MODEL_HODGKIN_HUXLEY ||
        neuron_model_from_name("unknown", &parsed_model) ||
        strcmp(neuron_model_name(MINISNN_NEURON_MODEL_HODGKIN_HUXLEY),
               "hodgkin_huxley") != 0)
        return 1;

    signature = neuron_model_config_signature(&model_config);
    changed_config = model_config;
    if (signature == 0ULL ||
        signature != neuron_model_config_signature(&changed_config))
        return 1;
    changed_config.data.lif.tau += 0.5;
    if (signature == neuron_model_config_signature(&changed_config))
        return 1;

    signature_config = minisnn_default_config();
    public_signature = minisnn_config_neuron_model_signature(&signature_config);
    signature_config.adex.b += 1.0;
    if (public_signature == 0ULL ||
        public_signature !=
            minisnn_config_neuron_model_signature(&signature_config))
        return 1;
    signature_config.tau += 1.0;
    if (public_signature ==
        minisnn_config_neuron_model_signature(&signature_config))
        return 1;

    lif_init_with_parameters(&direct, &parameters);
    neuron_model_init(&dispatched, &model_config);
    for (int step = 0; step < 500; step++)
    {
        double current = step < 300 ? 20.0 : 0.0;
        int expected = lif_update_with_parameters(&direct, current, &parameters);
        NeuronStepContext context = {current, 0, 0.0};
        int actual = neuron_model_step(&dispatched, &model_config, &context);
        if (expected != actual || fabs(direct.V - neuron_model_voltage(
                &dispatched)) > 1e-12 ||
            direct.spike != neuron_model_spike(&dispatched))
            return 1;
    }
    neuron_model_reset(&dispatched, &model_config);
    if (fabs(dispatched.V - parameters.v_rest) > 1e-12 || dispatched.spike != 0)
        return 1;

    dispatched.type = NEURON_INHIBITORY;
    dispatched.model = MINISNN_NEURON_MODEL_LIF;
    dispatched.V = 10.0;
    dispatched.spike = 1;
    neuron_model_reset(&dispatched, &model_config);
    if (dispatched.type != NEURON_INHIBITORY ||
        dispatched.model != MINISNN_NEURON_MODEL_LIF ||
        fabs(dispatched.V - parameters.v_rest) > 1e-12 ||
        dispatched.spike != 0)
        return 1;

    first = minisnn_create_with_config(&config);
    second = minisnn_create_with_config(&config);
    if (first == NULL || second == NULL ||
        minisnn_neuron_model(first) != MINISNN_NEURON_MODEL_LIF ||
        minisnn_neuron_model_config_signature(first) !=
            minisnn_config_neuron_model_signature(&config))
    {
        minisnn_destroy(&first);
        minisnn_destroy(&second);
        return 1;
    }

    lif_init_with_parameters(&api_reference, &parameters);
    for (int step = 0; step < 200; step++)
    {
        double voltage;
        int expected;

        minisnn_clear_inputs(first);
        if (!minisnn_set_input(first, 0, 20.0))
        {
            minisnn_destroy(&first);
            minisnn_destroy(&second);
            return 1;
        }

        expected = lif_update_with_parameters(&api_reference, 20.0, &parameters);
        if (minisnn_step(first) != expected ||
            !minisnn_get_voltage(first, 0, &voltage) ||
            fabs(voltage - api_reference.V) > 1e-12 ||
            minisnn_step(second) != 0 ||
            !minisnn_get_voltage(second, 0, &voltage) ||
            fabs(voltage - parameters.v_rest) > 1e-12)
        {
            minisnn_destroy(&first);
            minisnn_destroy(&second);
            return 1;
        }
    }

    minisnn_destroy(&first);
    minisnn_destroy(&second);

    if (!check_public_effective_dt(
            MINISNN_NEURON_MODEL_ADEX, 0.1, 0.2, 500.0) ||
        !check_public_effective_dt(
            MINISNN_NEURON_MODEL_HODGKIN_HUXLEY, 0.01, 0.02, 10.0))
        return 1;

    for (int model = MINISNN_NEURON_MODEL_LIF;
         model <= MINISNN_NEURON_MODEL_HODGKIN_HUXLEY;
         model++)
    {
        MiniSNNConfig topology_config = minisnn_default_config();
        topology_config.neuron_count = 2;
        topology_config.neuron_model = (MiniSNNNeuronModel)model;
        first = minisnn_create_with_config(&topology_config);
        if (first == NULL || !minisnn_connect(first, 0, 1, 1.0) ||
            !minisnn_get_topology_signature(
                first, &topology_signatures[model]))
        {
            minisnn_destroy(&first);
            return 1;
        }
        minisnn_destroy(&first);
    }
    if (topology_signatures[MINISNN_NEURON_MODEL_LIF] ==
            topology_signatures[MINISNN_NEURON_MODEL_ADEX] ||
        topology_signatures[MINISNN_NEURON_MODEL_LIF] ==
            topology_signatures[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY] ||
        topology_signatures[MINISNN_NEURON_MODEL_ADEX] ==
            topology_signatures[MINISNN_NEURON_MODEL_HODGKIN_HUXLEY])
        return 1;

    printf("Neuron model interface validation OK\n");
    return 0;
}

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "homeostasis.h"
#include "minisnn.h"

#define EPSILON 1e-9

static void require(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "Homeostasis test failed: %s\n", message);
        exit(1);
    }
}

static void require_close(double actual, double expected, const char *message)
{
    if (!isfinite(actual) || fabs(actual - expected) > EPSILON)
    {
        fprintf(
            stderr,
            "Homeostasis test failed: %s (actual=%.17g expected=%.17g)\n",
            message,
            actual,
            expected);
        exit(1);
    }
}

static MiniSNNConfig unit_config(int neurons)
{
    MiniSNNConfig config = minisnn_default_config();
    config.neuron_count = neurons;
    config.dt = 1.0;
    config.tau = 1.0;
    config.v_rest = -65.0;
    config.v_reset = -65.0;
    config.v_threshold = -50.0;
    config.resistance = 1.0;
    config.synaptic_decay = 0.0;
    config.max_synaptic_delay = 2;
    return config;
}

static void test_pure_formulas(void)
{
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    double alpha = exp(-1.0);
    double rate;

    require(config.enabled == 0, "default must be disabled");
    rate = homeostasis_rate_trace_next(0.0, 1, 1.0, 1.0);
    require_close(rate, 1.0 - alpha, "exact rate trace with spike");
    rate = homeostasis_rate_trace_next(rate, 0, 1.0, 1.0);
    require_close(rate, alpha * (1.0 - alpha), "exact rate trace decay");

    config.target_rate = 0.5;
    config.threshold_eta = 2.0;
    config.threshold_min = -60.0;
    config.threshold_max = -40.0;
    require_close(
        homeostasis_threshold_next(-50.0, 1.0, &config),
        -49.0,
        "threshold increases above target");
    require_close(
        homeostasis_threshold_next(-50.0, 0.0, &config),
        -51.0,
        "threshold decreases below target");
    require_close(
        homeostasis_threshold_next(-40.1, 1.0, &config),
        -40.0,
        "threshold maximum clamp");
    require_close(
        homeostasis_threshold_next(-59.9, 0.0, &config),
        -60.0,
        "threshold minimum clamp");

    config.scaling_eta = 0.5;
    config.scaling_min_factor = 0.1;
    config.scaling_max_factor = 3.0;
    require_close(
        homeostasis_scaling_factor(100.0, 200.0, &config),
        0.75,
        "exact scaling factor");
    require(isnan(homeostasis_scaling_factor(100.0, 0.0, &config)),
        "zero current sum must not divide");

    config.target_rate = 0.25;
    config.inhibitory_gain_eta = 2.0;
    config.inhibitory_gain_min = 0.5;
    config.inhibitory_gain_max = 2.0;
    require_close(
        homeostasis_inhibitory_gain_next(1.0, 0.5, &config),
        1.5,
        "inhibitory gain increase");
    require_close(
        homeostasis_inhibitory_gain_next(1.0, 0.0, &config),
        0.5,
        "inhibitory gain decrease and clamp");
}

static void test_threshold_order_and_reset(void)
{
    MiniSNNConfig network_config = unit_config(1);
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNNHomeostasisStats stats;
    MiniSNN *snn = minisnn_create_with_config(&network_config);
    double threshold;
    double rate;
    int spike;

    require(snn != NULL, "create threshold network");
    config.enabled = 1;
    config.intrinsic_enabled = 1;
    config.target_rate = 0.0;
    config.rate_tau = 1.0;
    config.update_interval_steps = 1U;
    config.threshold_eta = 1.0;
    require(minisnn_set_homeostasis_config(snn, &config), "configure threshold");

    require(minisnn_set_input(snn, 0, 15.0), "set threshold input");
    require(minisnn_step(snn) == 1, "current spike uses old threshold");
    require(minisnn_get_spike(snn, 0, &spike) && spike == 1,
        "spike visible after threshold update");
    require(minisnn_get_neuron_rate_trace(snn, 0, &rate), "get rate trace");
    require_close(rate, 1.0 - exp(-1.0), "runtime rate trace");
    require(minisnn_get_neuron_effective_threshold(snn, 0, &threshold),
        "get effective threshold");
    require_close(threshold, -50.0 + rate, "threshold updated after spike");
    require(minisnn_get_homeostasis_stats(snn, &stats), "get stats");
    require(stats.update_count == 1ULL, "first update after one complete step");
    require(stats.threshold_increase_events == 1ULL, "threshold event counted");

    require(minisnn_step(snn) == 0,
        "next timestep uses the higher effective threshold");

    require(minisnn_reset_homeostasis(snn), "reset homeostasis");
    require(minisnn_get_neuron_rate_trace(snn, 0, &rate), "get reset trace");
    require_close(rate, 0.0, "reset clears trace");
    require(minisnn_get_neuron_effective_threshold(snn, 0, &threshold),
        "get reset threshold");
    require_close(threshold, -50.0, "reset restores base threshold");
    require(minisnn_current_step(snn) == 2, "reset preserves network step");
    minisnn_destroy(&snn);
}

static void test_scaling_and_weight_signs(void)
{
    MiniSNNConfig network_config = unit_config(3);
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNNHomeostasisStats stats;
    MiniSNN *snn = minisnn_create_with_config(&network_config);
    double weight0;
    double weight1;
    double initial_sum;
    double current_sum;

    require(snn != NULL, "create scaling network");
    require(minisnn_connect(snn, 0, 2, 30.0), "connect first EXC input");
    require(minisnn_connect(snn, 1, 2, 70.0), "connect second EXC input");

    config.enabled = 1;
    config.intrinsic_enabled = 0;
    config.synaptic_scaling_enabled = 1;
    config.scaling_eta = 0.5;
    config.scaling_min_factor = 0.1;
    config.scaling_max_factor = 3.0;
    config.update_interval_steps = 1U;
    require(minisnn_set_homeostasis_config(snn, &config), "configure scaling");
    require(minisnn_get_initial_incoming_exc_sum(snn, 2, &initial_sum),
        "get scaling target");
    require_close(initial_sum, 100.0, "captured initial incoming sum");

    require(minisnn_set_connection_weight(snn, 0, 60.0), "change first weight");
    require(minisnn_set_connection_weight(snn, 1, 140.0), "change second weight");
    require(minisnn_step(snn) == 0, "scaling step without spikes");
    require(minisnn_get_connection_weight(snn, 0, &weight0), "get scaled first");
    require(minisnn_get_connection_weight(snn, 1, &weight1), "get scaled second");
    require_close(weight0, 45.0, "first exact scaled weight");
    require_close(weight1, 105.0, "second exact scaled weight");
    require_close(weight1 / weight0, 140.0 / 60.0, "scaling preserves ratio");
    require(minisnn_get_current_incoming_exc_sum(snn, 2, &current_sum),
        "get current incoming sum");
    require_close(current_sum, 150.0, "current incoming sum after partial scaling");
    require(minisnn_get_homeostasis_stats(snn, &stats), "get scaling stats");
    require(stats.scaling_events == 1ULL, "one target scaled");
    require(stats.scaling_connections_modified == 2ULL,
        "two scaling connection changes counted");

    require(minisnn_reset_homeostasis(snn), "reset scaling state");
    require(minisnn_get_initial_incoming_exc_sum(snn, 2, &initial_sum),
        "get recaptured target");
    require_close(initial_sum, 150.0, "reset recaptures current weights");
    require(minisnn_get_connection_weight(snn, 0, &weight0), "weight preserved reset");
    require_close(weight0, 45.0, "reset preserves weights");
    minisnn_destroy(&snn);
}

static void test_inhibitory_gain_transmission(void)
{
    MiniSNNConfig network_config = unit_config(2);
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNN *snn = minisnn_create_with_config(&network_config);
    double current;
    double weight;
    double gain;

    require(snn != NULL, "create inhibitory network");
    require(minisnn_set_neuron_type(snn, 0, MINISNN_NEURON_INHIBITORY),
        "set inhibitory source");
    require(minisnn_connect(snn, 0, 1, -10.0), "connect inhibitory source");

    config.enabled = 1;
    config.intrinsic_enabled = 0;
    config.inhibitory_gain_enabled = 1;
    config.target_rate = 0.0;
    config.update_interval_steps = 1U;
    config.inhibitory_gain_initial = 1.0;
    config.inhibitory_gain_eta = 1.0;
    require(minisnn_set_homeostasis_config(snn, &config), "configure gain");
    require(minisnn_set_input(snn, 0, 20.0), "stimulate inhibitory source");
    require(minisnn_step(snn) == 1, "inhibitory source spike");
    require(minisnn_get_inhibitory_gain(snn, &gain), "get updated gain");
    require(gain > 1.0, "gain updates after current transmission is scheduled");
    minisnn_clear_inputs(snn);
    require(minisnn_step(snn) >= 0, "deliver inhibitory current");
    require(minisnn_get_synaptic_current(snn, 1, &current), "get used current");
    require_close(current, -10.0, "old gain used in current transmission");
    require(minisnn_get_connection_weight(snn, 0, &weight), "get raw INH weight");
    require_close(weight, -10.0, "gain does not modify raw inhibitory weight");
    minisnn_destroy(&snn);
}

static void test_stdp_then_scaling(void)
{
    MiniSNNConfig network_config = unit_config(2);
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNNPlasticityStats plasticity_stats;
    MiniSNNHomeostasisStats homeostasis_stats;
    MiniSNN *snn = minisnn_create_with_config(&network_config);
    double weight;

    require(snn != NULL, "create STDP and scaling network");
    require(minisnn_connect(snn, 0, 1, 10.0), "connect STDP pair");
    plasticity.enabled = 1;
    plasticity.a_plus = 2.0;
    plasticity.a_minus = 0.0;
    plasticity.tau_plus = 2.0;
    plasticity.tau_minus = 2.0;
    plasticity.weight_max = 100.0;
    require(minisnn_set_plasticity_config(snn, &plasticity),
        "configure STDP before scaling");

    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 0;
    homeostasis.synaptic_scaling_enabled = 1;
    homeostasis.scaling_eta = 1.0;
    homeostasis.scaling_min_factor = 0.1;
    homeostasis.scaling_max_factor = 2.0;
    homeostasis.update_interval_steps = 1U;
    require(minisnn_set_homeostasis_config(snn, &homeostasis),
        "configure scaling after STDP");

    require(minisnn_set_input(snn, 0, 20.0), "emit pre spike");
    require(minisnn_step(snn) == 1, "pre step");
    minisnn_clear_inputs(snn);
    require(minisnn_set_input(snn, 1, 15.0), "emit post spike");
    require(minisnn_step(snn) == 1, "post step");

    require(minisnn_get_plasticity_stats(snn, &plasticity_stats),
        "get STDP stats with scaling");
    require(minisnn_get_homeostasis_stats(snn, &homeostasis_stats),
        "get scaling stats after STDP");
    require(plasticity_stats.potentiation_events > 0ULL,
        "STDP potentiation happened before scaling");
    require(homeostasis_stats.scaling_connections_modified > 0ULL,
        "scaling changed the STDP result");
    require(minisnn_get_connection_weight(snn, 0, &weight),
        "get final STDP and scaling weight");
    require_close(weight, 10.0, "full scaling restores captured target after STDP");
    minisnn_destroy(&snn);
}

static void test_public_api_validation_and_edges(void)
{
    MiniSNNHomeostasisConfig valid = minisnn_default_homeostasis_config();
    MiniSNNHomeostasisConfig invalid;
    MiniSNNHomeostasisConfig observed;
    MiniSNNHomeostasisStats stats;
    MiniSNN *snn = minisnn_create(1);
    double value;

    require(snn != NULL, "create API validation network");
    valid.enabled = 1;
    valid.intrinsic_enabled = 1;
    require(minisnn_set_homeostasis_config(snn, &valid), "valid public config");
    require(minisnn_get_homeostasis_config(snn, &observed), "get public config");
    require(observed.enabled == 1, "public config enabled");

#define REJECT_CONFIG(statement, message) \
    do { invalid = valid; statement; \
         require(!minisnn_set_homeostasis_config(snn, &invalid), message); } while (0)
    REJECT_CONFIG(invalid.enabled = 2, "reject invalid enabled boolean");
    REJECT_CONFIG(invalid.target_rate = NAN, "reject NaN target rate");
    REJECT_CONFIG(invalid.rate_tau = 0.0, "reject zero rate tau");
    REJECT_CONFIG(invalid.update_interval_steps = 0U, "reject zero interval");
    REJECT_CONFIG(invalid.threshold_max = invalid.threshold_min,
        "reject inverted threshold bounds");
    REJECT_CONFIG(invalid.scaling_eta = 1.1, "reject scaling eta above one");
    REJECT_CONFIG(invalid.scaling_min_factor = 0.0, "reject zero scaling factor");
    REJECT_CONFIG(invalid.scaling_weight_min = -1.0, "reject negative scaling weight");
    REJECT_CONFIG(invalid.inhibitory_gain_initial = 0.0, "reject zero gain");
    REJECT_CONFIG(invalid.inhibitory_gain_eta = INFINITY, "reject infinite gain eta");
    REJECT_CONFIG(invalid.intrinsic_enabled = 0;
                  invalid.synaptic_scaling_enabled = 0;
                  invalid.inhibitory_gain_enabled = 0,
        "reject enabled config without mechanism");
#undef REJECT_CONFIG

    require(minisnn_get_homeostasis_config(snn, &observed),
        "get config after rejected changes");
    require(observed.enabled == 1 && observed.intrinsic_enabled == 1,
        "invalid config does not replace previous config");
    require(!minisnn_set_homeostasis_config(NULL, &valid), "reject null network");
    require(!minisnn_set_homeostasis_config(snn, NULL), "reject null config");
    require(!minisnn_get_homeostasis_config(NULL, &observed), "reject null config getter");
    require(!minisnn_get_homeostasis_stats(snn, NULL), "reject null stats output");
    require(!minisnn_get_neuron_rate_trace(snn, -1, &value), "reject negative rate ID");
    require(!minisnn_get_neuron_rate_trace(snn, 1, &value), "reject high rate ID");
    require(!minisnn_get_neuron_effective_threshold(snn, 0, NULL),
        "reject null threshold output");
    require(!minisnn_get_initial_incoming_exc_sum(snn, 1, &value),
        "reject high incoming-sum ID");

    require(minisnn_step(snn) == 0, "one-neuron zero-connection step");
    require(minisnn_get_current_incoming_exc_sum(snn, 0, &value),
        "zero incoming EXC sum getter");
    require_close(value, 0.0, "zero-connection incoming sum");
    require(minisnn_get_homeostasis_stats(snn, &stats), "edge network stats");
    require(stats.update_count == 0ULL, "default interval not reached in edge network");

    minisnn_destroy(&snn);
    require(snn == NULL, "public destroy clears pointer");
    require(!minisnn_reset_homeostasis(snn), "reset rejects destroyed network");
}

static void test_scaling_zero_sum_and_self_connection(void)
{
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNNHomeostasisStats stats;
    MiniSNN *snn = minisnn_create(1);
    double weight;

    require(snn != NULL, "create self-connection scaling network");
    require(minisnn_connect_ex(snn, 0, 0, 10.0, 1), "create explicit self connection");
    config.enabled = 1;
    config.intrinsic_enabled = 0;
    config.synaptic_scaling_enabled = 1;
    config.update_interval_steps = 1U;
    require(minisnn_set_homeostasis_config(snn, &config), "configure self scaling");
    require(minisnn_set_connection_weight(snn, 0, 0.0), "zero captured connection");
    require(minisnn_step(snn) == 0, "safe zero-sum scaling step");
    require(minisnn_get_homeostasis_stats(snn, &stats), "get zero-sum stats");
    require(stats.scaling_zero_sum_skips == 1ULL, "zero-sum skip counted");
    require(minisnn_get_connection_weight(snn, 0, &weight), "get zero weight");
    require_close(weight, 0.0, "zero weight not recreated");
    minisnn_destroy(&snn);
}

static void test_independent_and_disabled_networks(void)
{
    MiniSNNConfig network_config = unit_config(1);
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNN *first = minisnn_create_with_config(&network_config);
    MiniSNN *second = minisnn_create_with_config(&network_config);
    MiniSNNHomeostasisStats stats;
    double first_rate;
    double second_rate;
    double threshold;
    double gain;

    require(first != NULL && second != NULL, "create independent networks");
    require(minisnn_get_neuron_effective_threshold(first, 0, &threshold),
        "disabled threshold getter");
    require_close(threshold, -50.0, "disabled threshold fixed");
    require(minisnn_get_inhibitory_gain(first, &gain), "disabled gain getter");
    require_close(gain, 1.0, "disabled gain is one");
    require(minisnn_get_homeostasis_stats(first, &stats), "disabled stats getter");
    require(stats.update_count == 0ULL, "disabled stats remain zero");

    config.enabled = 1;
    config.target_rate = 0.0;
    config.rate_tau = 1.0;
    config.update_interval_steps = 1U;
    require(minisnn_set_homeostasis_config(first, &config), "configure first only");
    require(minisnn_set_homeostasis_config(second, &config), "configure second");
    require(minisnn_set_input(first, 0, 20.0), "stimulate first only");
    require(minisnn_step(first) == 1, "step first");
    require(minisnn_get_neuron_rate_trace(first, 0, &first_rate), "first rate");
    require(minisnn_get_neuron_rate_trace(second, 0, &second_rate), "second rate");
    require(first_rate > 0.0, "first rate changed");
    require_close(second_rate, 0.0, "second rate independent");

    minisnn_destroy(&first);
    minisnn_destroy(&second);
}

static void test_model_capability_validation(void)
{
    MiniSNNConfig network_config = minisnn_default_config();
    MiniSNNHomeostasisConfig config = minisnn_default_homeostasis_config();
    MiniSNN *snn;
    double threshold;

    network_config.neuron_count = 2;
    network_config.neuron_model = MINISNN_NEURON_MODEL_ADEX;
    network_config.dt = network_config.adex.dt;
    snn = minisnn_create_with_config(&network_config);
    require(snn != NULL, "create AdEx capability network");

    config.enabled = 1;
    config.intrinsic_enabled = 1;
    require(!minisnn_set_homeostasis_config(snn, &config),
        "AdEx rejects intrinsic homeostasis at configuration");
    require(!minisnn_get_neuron_effective_threshold(snn, 0, &threshold),
        "AdEx threshold accessor rejects unsupported capability");

    config.intrinsic_enabled = 0;
    config.synaptic_scaling_enabled = 1;
    require(minisnn_set_homeostasis_config(snn, &config),
        "AdEx accepts scaling without threshold capability");
    config.synaptic_scaling_enabled = 0;
    config.inhibitory_gain_enabled = 1;
    require(minisnn_set_homeostasis_config(snn, &config),
        "AdEx accepts inhibitory gain without threshold capability");
    config.synaptic_scaling_enabled = 1;
    require(minisnn_set_homeostasis_config(snn, &config),
        "AdEx accepts scaling plus gain without threshold capability");
    minisnn_destroy(&snn);
}

int main(void)
{
    test_pure_formulas();
    test_threshold_order_and_reset();
    test_scaling_and_weight_signs();
    test_inhibitory_gain_transmission();
    test_stdp_then_scaling();
    test_public_api_validation_and_edges();
    test_scaling_zero_sum_and_self_connection();
    test_independent_and_disabled_networks();
    test_model_capability_validation();
    printf("Homeostasis numerical validation OK\n");
    return 0;
}

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"
#include "plasticity.h"
#include "reward.h"

#define TOLERANCE 1e-10

typedef struct
{
    LIFNeuron neurons[2];
    Connection edge;
    ConnectionList connections[2];
    PlasticityState plasticity;
    RewardState reward;
} RewardFixture;

static int configure_public_rstdp(
    MiniSNN *snn,
    double a_plus,
    double learning_rate);

static int nearly_equal(double actual, double expected)
{
    return fabs(actual - expected) <= TOLERANCE;
}

static int fixture_init(
    RewardFixture *fixture,
    double a_plus,
    double a_minus,
    double eligibility_tau,
    double learning_rate)
{
    MiniSNNPlasticityConfig plasticity_config;
    MiniSNNRewardConfig reward_config;

    memset(fixture, 0, sizeof(*fixture));
    lif_init(&fixture->neurons[0]);
    lif_init(&fixture->neurons[1]);
    fixture->edge.target = 1;
    fixture->edge.weight = 100.0;
    fixture->edge.delay = 1;
    fixture->connections[0].list = &fixture->edge;
    fixture->connections[0].count = 1;

    if (!plasticity_state_init(&fixture->plasticity, 2) ||
        !reward_state_init(&fixture->reward))
    {
        return 0;
    }

    plasticity_config = plasticity_default_config();
    plasticity_config.enabled = 1;
    plasticity_config.learning_mode =
        MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    plasticity_config.a_plus = a_plus;
    plasticity_config.a_minus = a_minus;
    plasticity_config.tau_plus = 2.0;
    plasticity_config.tau_minus = 2.0;
    plasticity_config.weight_min = 0.0;
    plasticity_config.weight_max = 200.0;

    if (!plasticity_state_configure(
            &fixture->plasticity,
            &plasticity_config,
            fixture->neurons,
            fixture->connections))
    {
        return 0;
    }

    reward_config = reward_default_config();
    reward_config.enabled = 1;
    reward_config.learning_rate = learning_rate;
    reward_config.eligibility_tau = eligibility_tau;
    return reward_state_configure(
        &fixture->reward,
        &reward_config,
        fixture->neurons,
        fixture->connections,
        &fixture->plasticity);
}

static void fixture_destroy(RewardFixture *fixture)
{
    reward_state_destroy(&fixture->reward);
    plasticity_state_destroy(&fixture->plasticity);
}

static int fixture_temporal_step(
    RewardFixture *fixture,
    int pre_spike,
    int post_spike,
    unsigned long long step)
{
    int spikes[2] = {pre_spike, post_spike};

    return plasticity_apply_step(
               &fixture->plasticity,
               fixture->neurons,
               fixture->connections,
               spikes,
               1.0) &&
        reward_state_accumulate_candidates(
            &fixture->reward,
            &fixture->plasticity,
            step,
            1.0);
}

static int test_exact_eligibility_signs(void)
{
    RewardFixture fixture;
    MiniSNNRewardConnectionStats connection_stats;
    double expected = 0.5 * exp(-0.5);

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 0.5))
        return 0;

    if (!fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !reward_state_get_connection_stats(
            &fixture.reward, 0, 1, 1.0, &connection_stats) ||
        !nearly_equal(fixture.edge.weight, 100.0) ||
        !nearly_equal(connection_stats.eligibility, expected))
    {
        fixture_destroy(&fixture);
        return 0;
    }
    fixture_destroy(&fixture);

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 0.5) ||
        !fixture_temporal_step(&fixture, 0, 1, 0) ||
        !fixture_temporal_step(&fixture, 1, 0, 1) ||
        !reward_state_get_connection_stats(
            &fixture.reward, 0, 1, 1.0, &connection_stats) ||
        !nearly_equal(
            connection_stats.eligibility,
            -0.75 * exp(-0.5)))
    {
        fixture_destroy(&fixture);
        return 0;
    }

    fixture_destroy(&fixture);
    return 1;
}

static int test_lazy_decay_and_reward_signs(void)
{
    RewardFixture fixture;
    MiniSNNRewardConnectionStats connection_stats;
    double initial_eligibility;
    double decayed;
    double expected_weight;

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 0.5) ||
        !fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !reward_state_get_connection_stats(
            &fixture.reward, 0, 1, 1.0, &connection_stats))
    {
        return 0;
    }

    initial_eligibility = connection_stats.eligibility;
    decayed = initial_eligibility * exp(-3.0 / 4.0);
    if (!reward_state_get_connection_stats(
            &fixture.reward, 0, 4, 1.0, &connection_stats) ||
        !nearly_equal(connection_stats.eligibility, decayed))
    {
        fixture_destroy(&fixture);
        return 0;
    }

    expected_weight = 100.0 + 0.5 * 0.8 * decayed;
    if (!reward_state_queue(&fixture.reward, 0.8) ||
        !reward_state_apply_pending(
            &fixture.reward,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity,
            4,
            1.0) ||
        !nearly_equal(fixture.edge.weight, expected_weight))
    {
        fixture_destroy(&fixture);
        return 0;
    }
    fixture_destroy(&fixture);

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 0.5) ||
        !fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !reward_state_get_connection_stats(
            &fixture.reward, 0, 1, 1.0, &connection_stats) ||
        !reward_state_queue(&fixture.reward, -0.8))
    {
        return 0;
    }
    expected_weight = 100.0 - 0.5 * 0.8 * connection_stats.eligibility;
    if (!reward_state_apply_pending(
            &fixture.reward,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity,
            1,
            1.0) ||
        !nearly_equal(fixture.edge.weight, expected_weight))
    {
        fixture_destroy(&fixture);
        return 0;
    }
    fixture_destroy(&fixture);

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 0.5) ||
        !fixture_temporal_step(&fixture, 0, 1, 0) ||
        !fixture_temporal_step(&fixture, 1, 0, 1) ||
        !reward_state_get_connection_stats(
            &fixture.reward, 0, 1, 1.0, &connection_stats) ||
        connection_stats.eligibility >= 0.0 ||
        !reward_state_queue(&fixture.reward, -0.8) ||
        !reward_state_apply_pending(
            &fixture.reward,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity,
            1,
            1.0) ||
        fixture.edge.weight <= 100.0)
    {
        fixture_destroy(&fixture);
        return 0;
    }

    fixture_destroy(&fixture);
    return 1;
}

static int test_queue_clamps_and_consumption(void)
{
    RewardFixture fixture;
    MiniSNNRewardConfig config;
    MiniSNNRewardStats stats;
    double weight_after_reward;

    if (!fixture_init(&fixture, 10.0, 10.0, 4.0, 1.0) ||
        !fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !reward_state_queue(&fixture.reward, 0.7) ||
        !reward_state_queue(&fixture.reward, 0.6) ||
        !reward_state_apply_pending(
            &fixture.reward,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity,
            1,
            1.0) ||
        !reward_state_get_stats(&fixture.reward, 1, 1.0, &stats) ||
        !nearly_equal(stats.last_raw_reward, 1.3) ||
        !nearly_equal(stats.last_applied_reward, 1.0) ||
        stats.last_reward_component_count != 2)
    {
        fixture_destroy(&fixture);
        return 0;
    }

    weight_after_reward = fixture.edge.weight;
    if (!reward_state_apply_pending(
            &fixture.reward,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity,
            2,
            1.0) ||
        !nearly_equal(fixture.edge.weight, weight_after_reward))
    {
        fixture_destroy(&fixture);
        return 0;
    }

    config = fixture.reward.config;
    config.clip_reward = 0;
    if (!reward_state_configure(
            &fixture.reward,
            &config,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity) ||
        !reward_state_queue(&fixture.reward, 0.7) ||
        reward_state_queue(&fixture.reward, 0.4) ||
        !nearly_equal(fixture.reward.pending_raw_reward, 0.7))
    {
        fixture_destroy(&fixture);
        return 0;
    }

    config = fixture.reward.config;
    config.eligibility_min = -0.1;
    config.eligibility_max = 0.1;
    if (!reward_state_configure(
            &fixture.reward,
            &config,
            fixture.neurons,
            fixture.connections,
            &fixture.plasticity) ||
        !fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !nearly_equal(fixture.reward.connections[0].eligibility, 0.1))
    {
        fixture_destroy(&fixture);
        return 0;
    }

    fixture_destroy(&fixture);
    return 1;
}

static int test_zero_weight_clamps_and_direct_mode(void)
{
    RewardFixture fixture;
    MiniSNNRewardStats stats;
    MiniSNNPlasticityConfig direct;
    int pre[2] = {1, 0};
    int post[2] = {0, 1};
    double expected;

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 1.0) ||
        !fixture_temporal_step(&fixture, 1, 0, 0) ||
        !fixture_temporal_step(&fixture, 0, 1, 1) ||
        !reward_state_queue(&fixture.reward, 0.0) ||
        !reward_state_apply_pending(
            &fixture.reward, fixture.neurons, fixture.connections,
            &fixture.plasticity, 1, 1.0) ||
        !nearly_equal(fixture.edge.weight, 100.0) ||
        !reward_state_get_stats(&fixture.reward, 1, 1.0, &stats) ||
        stats.zero_reward_event_count != 1)
    {
        fixture_destroy(&fixture);
        return 0;
    }

    fixture.edge.weight = 199.9;
    if (!reward_state_queue(&fixture.reward, 1.0) ||
        !reward_state_apply_pending(
            &fixture.reward, fixture.neurons, fixture.connections,
            &fixture.plasticity, 1, 1.0) ||
        !nearly_equal(fixture.edge.weight, 200.0) ||
        !reward_state_get_stats(&fixture.reward, 1, 1.0, &stats) ||
        stats.weight_clamp_max_events == 0)
    {
        fixture_destroy(&fixture);
        return 0;
    }

    fixture.edge.weight = 0.1;
    if (!reward_state_queue(&fixture.reward, -1.0) ||
        !reward_state_apply_pending(
            &fixture.reward, fixture.neurons, fixture.connections,
            &fixture.plasticity, 1, 1.0) ||
        !nearly_equal(fixture.edge.weight, 0.0) ||
        !reward_state_get_stats(&fixture.reward, 1, 1.0, &stats) ||
        stats.weight_clamp_min_events == 0)
    {
        fixture_destroy(&fixture);
        return 0;
    }
    fixture_destroy(&fixture);

    if (!fixture_init(&fixture, 0.5, 0.75, 4.0, 1.0))
        return 0;
    direct = fixture.plasticity.config;
    direct.learning_mode = MINISNN_LEARNING_MODE_DIRECT_STDP;
    if (!plasticity_state_configure(
            &fixture.plasticity, &direct, fixture.neurons,
            fixture.connections) ||
        !plasticity_apply_step(
            &fixture.plasticity, fixture.neurons, fixture.connections,
            pre, 1.0) ||
        !plasticity_apply_step(
            &fixture.plasticity, fixture.neurons, fixture.connections,
            post, 1.0))
    {
        fixture_destroy(&fixture);
        return 0;
    }
    expected = 100.0 + 0.5 * exp(-0.5);
    if (!nearly_equal(fixture.edge.weight, expected))
    {
        fixture_destroy(&fixture);
        return 0;
    }
    fixture_destroy(&fixture);
    return 1;
}

static int test_inhibitory_and_self_connection(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *snn;
    MiniSNNPlasticityConfig plasticity;
    MiniSNNRewardConfig reward;
    MiniSNNRewardConnectionStats stats;
    double weight;

    config.neuron_count = 2;
    config.dt = 1.0;
    config.tau = 1.0;
    config.v_rest = 0.0;
    config.v_reset = 0.0;
    config.v_threshold = 1.0;
    config.resistance = 1.0;
    config.synaptic_decay = 0.0;
    snn = minisnn_create_with_config(&config);
    if (snn == NULL ||
        !minisnn_set_neuron_type(snn, 0, MINISNN_NEURON_INHIBITORY) ||
        !minisnn_connect(snn, 0, 1, -2.0) ||
        !configure_public_rstdp(snn, 0.5, 1.0) ||
        !minisnn_set_input(snn, 0, 2.0) || minisnn_step(snn) != 1 ||
        !minisnn_queue_reward(snn, 1.0) || minisnn_step(snn) < 0 ||
        !minisnn_get_reward_connection_stats(snn, 0, &stats) ||
        stats.eligible ||
        !minisnn_get_connection_weight(snn, 0, &weight) ||
        !nearly_equal(weight, -2.0))
    {
        minisnn_destroy(&snn);
        return 0;
    }
    minisnn_destroy(&snn);

    config.neuron_count = 1;
    snn = minisnn_create_with_config(&config);
    plasticity = minisnn_default_plasticity_config();
    reward = minisnn_default_reward_config();
    plasticity.enabled = 1;
    plasticity.learning_mode = MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    plasticity.a_plus = 1.0;
    plasticity.a_minus = 0.25;
    plasticity.tau_plus = 10.0;
    plasticity.tau_minus = 10.0;
    plasticity.weight_max = 10.0;
    reward.enabled = 1;
    if (snn == NULL || !minisnn_connect_ex(snn, 0, 0, 1.0, 1) ||
        !minisnn_set_plasticity_config(snn, &plasticity) ||
        !minisnn_set_reward_config(snn, &reward) ||
        !minisnn_set_input(snn, 0, 2.0) || minisnn_step(snn) != 1 ||
        !minisnn_set_input(snn, 0, 2.0) ||
        !minisnn_queue_reward(snn, 1.0) || minisnn_step(snn) != 1 ||
        !minisnn_get_connection_weight(snn, 0, &weight) || weight <= 1.0)
    {
        minisnn_destroy(&snn);
        return 0;
    }
    minisnn_destroy(&snn);
    return 1;
}

static int configure_public_rstdp(
    MiniSNN *snn,
    double a_plus,
    double learning_rate)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();

    plasticity.enabled = 1;
    plasticity.learning_mode =
        MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    plasticity.a_plus = a_plus;
    plasticity.a_minus = a_plus;
    plasticity.tau_plus = 2.0;
    plasticity.tau_minus = 2.0;
    plasticity.weight_max = 10.0;
    reward.enabled = 1;
    reward.learning_rate = learning_rate;
    reward.eligibility_tau = 1000.0;

    return minisnn_set_plasticity_config(snn, &plasticity) &&
        minisnn_set_reward_config(snn, &reward);
}

static int test_public_timing_and_reset(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *snn;
    MiniSNNRewardStats reward_stats;
    MiniSNNHomeostasisConfig homeostasis;
    MiniSNNHomeostasisStats homeostasis_before;
    MiniSNNHomeostasisStats homeostasis_after;
    double weight_after_reward;
    double current;
    double pending;

    config.neuron_count = 2;
    config.dt = 1.0;
    config.tau = 1.0;
    config.v_rest = 0.0;
    config.v_reset = 0.0;
    config.v_threshold = 1.0;
    config.resistance = 1.0;
    config.synaptic_decay = 0.0;
    config.max_synaptic_delay = 2;
    snn = minisnn_create_with_config(&config);

    if (snn == NULL || !minisnn_connect(snn, 0, 1, 1.0) ||
        !configure_public_rstdp(snn, 0.5, 1.0) ||
        !minisnn_set_input(snn, 0, 2.0) || minisnn_step(snn) != 1)
    {
        minisnn_destroy(&snn);
        return 0;
    }

    minisnn_clear_inputs(snn);
    if (!minisnn_queue_reward(snn, 1.0) || minisnn_step(snn) != 1 ||
        !minisnn_get_synaptic_current(snn, 1, &current) ||
        !nearly_equal(current, 1.0) ||
        !minisnn_get_connection_weight(snn, 0, &weight_after_reward) ||
        weight_after_reward <= 1.0)
    {
        minisnn_destroy(&snn);
        return 0;
    }

    minisnn_clear_inputs(snn);
    if (!minisnn_set_input(snn, 0, 2.0) || minisnn_step(snn) != 1)
    {
        minisnn_destroy(&snn);
        return 0;
    }
    minisnn_clear_inputs(snn);
    if (minisnn_step(snn) < 0 ||
        !minisnn_get_synaptic_current(snn, 1, &current) ||
        !nearly_equal(current, weight_after_reward))
    {
        minisnn_destroy(&snn);
        return 0;
    }

    homeostasis = minisnn_default_homeostasis_config();
    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 1;
    homeostasis.target_rate = 0.0;
    homeostasis.threshold_min = 0.5;
    homeostasis.threshold_max = 2.0;
    if (!minisnn_set_homeostasis_config(snn, &homeostasis) ||
        !minisnn_get_homeostasis_stats(snn, &homeostasis_before) ||
        !minisnn_queue_reward(snn, 0.5) ||
        !minisnn_reset_reward_learning(snn) ||
        !minisnn_get_pending_reward(snn, &pending) ||
        !nearly_equal(pending, 0.0) ||
        !minisnn_get_reward_stats(snn, &reward_stats) ||
        reward_stats.reward_event_count != 0 ||
        !minisnn_get_homeostasis_stats(snn, &homeostasis_after) ||
        homeostasis_before.update_count != homeostasis_after.update_count)
    {
        minisnn_destroy(&snn);
        return 0;
    }

    minisnn_destroy(&snn);
    return snn == NULL;
}

static int test_reward_then_scaling_order(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *snn;
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    MiniSNNRewardStats reward_stats;
    MiniSNNHomeostasisStats homeostasis_stats;
    double final_weight;

    config.neuron_count = 2;
    config.dt = 1.0;
    config.tau = 1.0;
    config.v_rest = 0.0;
    config.v_reset = 0.0;
    config.v_threshold = 1.0;
    config.resistance = 1.0;
    config.synaptic_decay = 0.0;
    snn = minisnn_create_with_config(&config);

    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 0;
    homeostasis.synaptic_scaling_enabled = 1;
    homeostasis.inhibitory_gain_enabled = 0;
    homeostasis.update_interval_steps = 1;
    homeostasis.scaling_eta = 1.0;
    homeostasis.scaling_min_factor = 0.1;
    homeostasis.scaling_max_factor = 2.0;
    homeostasis.scaling_weight_min = 0.0;
    homeostasis.scaling_weight_max = 10.0;

    if (snn == NULL || !minisnn_connect(snn, 0, 1, 1.0) ||
        !configure_public_rstdp(snn, 0.5, 1.0) ||
        !minisnn_set_homeostasis_config(snn, &homeostasis) ||
        !minisnn_set_input(snn, 0, 2.0) || minisnn_step(snn) != 1)
    {
        minisnn_destroy(&snn);
        return 0;
    }

    minisnn_clear_inputs(snn);
    if (!minisnn_queue_reward(snn, 1.0) || minisnn_step(snn) != 1 ||
        !minisnn_get_connection_weight(snn, 0, &final_weight) ||
        !minisnn_get_reward_stats(snn, &reward_stats) ||
        !minisnn_get_homeostasis_stats(snn, &homeostasis_stats) ||
        reward_stats.total_signed_weight_change <= 0.0 ||
        !nearly_equal(
            homeostasis_stats.total_scaling_signed_change,
            -reward_stats.total_signed_weight_change) ||
        !nearly_equal(final_weight, 1.0))
    {
        minisnn_destroy(&snn);
        return 0;
    }

    minisnn_destroy(&snn);
    return 1;
}

static int test_public_validation_and_independence(void)
{
    MiniSNN *first = minisnn_create(2);
    MiniSNN *second = minisnn_create(2);
    MiniSNNRewardConfig config = minisnn_default_reward_config();
    MiniSNNRewardConfig invalid;
    double pending;
    size_t count;

    invalid = config;
    invalid.eligibility_tau = 0.0;
    if (first == NULL || second == NULL ||
        minisnn_set_reward_config(first, &invalid) ||
        minisnn_queue_reward(first, 1.0) ||
        !minisnn_get_pending_reward(second, &pending) ||
        !nearly_equal(pending, 0.0) ||
        !minisnn_reward_eligible_connection_count(second, &count) ||
        count != 0 || minisnn_get_reward_stats(NULL, NULL))
    {
        minisnn_destroy(&first);
        minisnn_destroy(&second);
        return 0;
    }

    minisnn_destroy(&first);
    minisnn_destroy(&second);
    return first == NULL && second == NULL;
}

int main(void)
{
    if (!test_exact_eligibility_signs() ||
        !test_lazy_decay_and_reward_signs() ||
        !test_queue_clamps_and_consumption() ||
        !test_zero_weight_clamps_and_direct_mode() ||
        !test_inhibitory_and_self_connection() ||
        !test_public_timing_and_reset() ||
        !test_reward_then_scaling_order() ||
        !test_public_validation_and_independence())
    {
        fprintf(stderr, "Reward-modulated STDP numerical validation FAILED\n");
        return 1;
    }

    printf("Reward-modulated STDP numerical validation OK\n");
    return 0;
}

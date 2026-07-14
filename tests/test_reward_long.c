#include <math.h>
#include <stdio.h>

#include "minisnn.h"

#define LONG_NEURONS 12
#define LONG_STEPS 20000

static int configure_network(MiniSNN *snn)
{
    MiniSNNPlasticityConfig plasticity = minisnn_default_plasticity_config();
    MiniSNNRewardConfig reward = minisnn_default_reward_config();
    MiniSNNHomeostasisConfig homeostasis = minisnn_default_homeostasis_config();
    int source;
    int target;

    for (source = 0; source < LONG_NEURONS; source++)
    {
        MiniSNNNeuronType type = source >= 10
            ? MINISNN_NEURON_INHIBITORY
            : MINISNN_NEURON_EXCITATORY;

        if (!minisnn_set_neuron_type(snn, source, type))
            return 0;

        for (target = 0; target < LONG_NEURONS; target++)
        {
            double weight;

            if (source == target)
                continue;
            weight = type == MINISNN_NEURON_INHIBITORY ? -350.0 : 120.0;
            if (!minisnn_connect_delayed(snn, source, target, weight, 2))
                return 0;
        }
    }

    plasticity.enabled = 1;
    plasticity.learning_mode =
        MINISNN_LEARNING_MODE_REWARD_MODULATED_STDP;
    plasticity.a_plus = 0.05;
    plasticity.a_minus = 0.055;
    plasticity.weight_min = 0.0;
    plasticity.weight_max = 300.0;

    reward.enabled = 1;
    reward.learning_rate = 0.25;
    reward.eligibility_tau = 100.0;
    reward.eligibility_min = -25.0;
    reward.eligibility_max = 25.0;

    homeostasis.enabled = 1;
    homeostasis.intrinsic_enabled = 1;
    homeostasis.target_rate = 0.02;
    homeostasis.update_interval_steps = 20;
    homeostasis.threshold_eta = 0.001;
    homeostasis.threshold_min = -60.0;
    homeostasis.threshold_max = -40.0;
    homeostasis.synaptic_scaling_enabled = 1;
    homeostasis.scaling_eta = 0.01;
    homeostasis.scaling_weight_min = 0.0;
    homeostasis.scaling_weight_max = 300.0;
    homeostasis.inhibitory_gain_enabled = 1;
    homeostasis.inhibitory_gain_min = 0.5;
    homeostasis.inhibitory_gain_max = 2.0;

    return minisnn_set_plasticity_config(snn, &plasticity) &&
        minisnn_set_reward_config(snn, &reward) &&
        minisnn_set_homeostasis_config(snn, &homeostasis);
}

int main(void)
{
    MiniSNNConfig config = minisnn_default_config();
    MiniSNN *snn;
    MiniSNNRewardStats reward_stats;
    MiniSNNHomeostasisStats homeostasis_stats;
    size_t initial_connections;
    size_t connection_id;
    double pending;
    double gain;
    int step;

    config.neuron_count = LONG_NEURONS;
    config.max_synaptic_delay = 4;
    snn = minisnn_create_with_config(&config);
    if (snn == NULL || !configure_network(snn))
        goto fail;

    initial_connections = minisnn_connection_count(snn);
    for (step = 0; step < LONG_STEPS; step++)
    {
        minisnn_clear_inputs(snn);
        if (!minisnn_set_input(snn, 0, 20.0) ||
            !minisnn_set_input(snn, 1, 20.0) ||
            !minisnn_set_input(snn, 2, 20.0))
        {
            goto fail;
        }

        if (step % 97 == 0 && !minisnn_queue_reward(snn, 0.75))
            goto fail;
        if (step % 131 == 0 && !minisnn_queue_reward(snn, -0.5))
            goto fail;
        if (minisnn_step(snn) < 0)
            goto fail;
    }

    if (minisnn_connection_count(snn) != initial_connections ||
        !minisnn_get_pending_reward(snn, &pending) || pending != 0.0 ||
        !minisnn_get_reward_stats(snn, &reward_stats) ||
        !minisnn_get_homeostasis_stats(snn, &homeostasis_stats) ||
        !minisnn_get_inhibitory_gain(snn, &gain) ||
        !isfinite(gain) || !isfinite(reward_stats.total_signed_weight_change) ||
        !isfinite(reward_stats.eligibility_final_mean) ||
        !isfinite(homeostasis_stats.final_population_rate))
    {
        goto fail;
    }

    for (connection_id = 0; connection_id < initial_connections; connection_id++)
    {
        MiniSNNConnectionInfo connection;
        MiniSNNRewardConnectionStats reward_connection;

        if (!minisnn_get_connection(snn, connection_id, &connection) ||
            !minisnn_get_reward_connection_stats(
                snn, connection_id, &reward_connection) ||
            !isfinite(connection.weight) ||
            !isfinite(reward_connection.eligibility) ||
            reward_connection.eligibility < -25.0 ||
            reward_connection.eligibility > 25.0)
        {
            goto fail;
        }

        if (connection.source_type == MINISNN_NEURON_EXCITATORY &&
            (connection.weight < 0.0 || connection.weight > 300.0))
        {
            goto fail;
        }
        if (connection.source_type == MINISNN_NEURON_INHIBITORY &&
            connection.weight != -350.0)
        {
            goto fail;
        }
    }

    minisnn_destroy(&snn);
    printf("Reward-modulated STDP long-run validation OK\n");
    return 0;

fail:
    minisnn_destroy(&snn);
    fprintf(stderr, "Reward-modulated STDP long-run validation FAILED\n");
    return 1;
}

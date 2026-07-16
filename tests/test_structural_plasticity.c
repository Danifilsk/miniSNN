#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "minisnn.h"

static int fail(const char *message)
{
    fprintf(stderr, "Structural plasticity test failed: %s\n", message);
    return 0;
}

static int same_double(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static MiniSNNStructuralPlasticityConfig base_config(void)
{
    MiniSNNStructuralPlasticityConfig config =
        minisnn_default_structural_plasticity_config();
    config.enabled = 1;
    config.maintenance_interval_steps = 1U;
    config.grace_period_steps = 1U;
    config.min_connections = 1U;
    config.max_connections = 6U;
    config.allow_self_connections = 0;
    config.allow_inh_to_inh = 1;
    return config;
}

static int check_defaults_and_validation(void)
{
    MiniSNN *snn = minisnn_create(3);
    MiniSNNStructuralPlasticityConfig config =
        minisnn_default_structural_plasticity_config();
    MiniSNNStructuralPlasticityConfig loaded;

    if (snn == NULL || config.enabled ||
        !minisnn_get_structural_plasticity_config(snn, &loaded) ||
        loaded.enabled ||
        !minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    config = base_config();
    if (minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    if (!minisnn_connect_delayed(snn, 0, 1, 0.1, 1))
        goto fail;
    config.maintenance_interval_steps = 0U;
    if (minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    config = base_config();
    config.grace_period_steps = 0U;
    if (minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    config = base_config();
    config.pruning_enabled = 0;
    config.growth_enabled = 0;
    if (minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    config = base_config();
    config.new_delay = 99U;
    if (minisnn_set_structural_plasticity_config(snn, &config))
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    minisnn_destroy(&snn);
    return fail("defaults or invalid configuration acceptance");
}

static int check_grace_pruning_and_reset(void)
{
    MiniSNN *snn = minisnn_create(3);
    MiniSNNStructuralPlasticityConfig config = base_config();
    MiniSNNStructuralStats stats;
    MiniSNNStructuralConnectionState state;
    uint64_t initial_signature;

    config.grace_period_steps = 2U;
    config.pruning_enabled = 1;
    config.growth_enabled = 0;
    config.prune_weight_threshold = 1.0;
    config.prune_activity_threshold = 1.0;
    config.max_prunes_per_interval = 1U;
    if (snn == NULL ||
        !minisnn_connect_delayed(snn, 0, 1, 0.1, 1) ||
        !minisnn_connect_delayed(snn, 1, 2, 0.2, 1) ||
        !minisnn_get_topology_signature(snn, &initial_signature) ||
        !minisnn_set_structural_plasticity_config(snn, &config) ||
        !minisnn_get_structural_connection_state(snn, 0U, &state) ||
        state.birth_step != 0U || state.growth_origin != 0U)
        goto fail;

    if (minisnn_step(snn) < 0 || minisnn_connection_count(snn) != 2U)
        goto fail;
    if (minisnn_step(snn) < 0 || minisnn_connection_count(snn) != 1U ||
        !minisnn_get_structural_stats(snn, &stats) ||
        stats.maintenance_count != 1U ||
        stats.remove_attempt_count != 1U ||
        stats.remove_success_count != 1U ||
        stats.rebuild_count != 1U ||
        stats.current_connection_count != 1U ||
        minisnn_structural_event_count(snn) != 1U)
        goto fail;

    if (!minisnn_reset_structural_plasticity(
            snn, MINISNN_STRUCTURAL_RESET_STATE) ||
        minisnn_connection_count(snn) != 1U ||
        minisnn_structural_event_count(snn) != 0U ||
        !minisnn_get_structural_stats(snn, &stats) ||
        stats.maintenance_count != 0U)
        goto fail;
    if (!minisnn_reset_structural_plasticity(
            snn, MINISNN_STRUCTURAL_RESTORE_INITIAL_TOPOLOGY) ||
        minisnn_connection_count(snn) != 2U ||
        !minisnn_get_structural_stats(snn, &stats) ||
        stats.current_topology_signature != initial_signature)
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    minisnn_destroy(&snn);
    return fail("grace period, pruning, state reset, or topology restore");
}

static int prepare_growth_network(
    MiniSNN **out_snn,
    uint64_t seed,
    size_t candidate_count,
    size_t growth_limit)
{
    MiniSNN *snn = minisnn_create(3);
    MiniSNNStructuralPlasticityConfig config = base_config();
    config.pruning_enabled = 0;
    config.growth_enabled = 1;
    config.growth_candidate_count = candidate_count;
    config.growth_score_threshold = 0.0;
    config.max_growth_per_interval = growth_limit;
    config.growth_seed = seed;
    config.new_exc_weight = 7.5;
    config.new_inh_magnitude = 8.5;
    config.new_delay = 2U;
    if (snn == NULL || !minisnn_connect_delayed(snn, 0, 1, 2.0, 1) ||
        !minisnn_set_structural_plasticity_config(snn, &config))
    {
        minisnn_destroy(&snn);
        return 0;
    }
    *out_snn = snn;
    return 1;
}

static int connections_equal(const MiniSNN *left, const MiniSNN *right)
{
    size_t count = minisnn_connection_count(left);
    if (count != minisnn_connection_count(right))
        return 0;
    for (size_t i = 0; i < count; i++)
    {
        MiniSNNConnectionInfo a;
        MiniSNNConnectionInfo b;
        if (!minisnn_get_connection(left, i, &a) ||
            !minisnn_get_connection(right, i, &b) ||
            a.source != b.source || a.target != b.target ||
            a.delay != b.delay || !same_double(a.weight, b.weight))
            return 0;
    }
    return 1;
}

static int check_growth_determinism_and_new_state(void)
{
    MiniSNN *first = NULL;
    MiniSNN *second = NULL;
    MiniSNN *batch = NULL;
    MiniSNNStructuralStats stats;
    MiniSNNStructuralConnectionState state;
    MiniSNNStructuralEvent event;
    int found_new_state = 0;

    if (!prepare_growth_network(&first, 77U, 1U, 1U) ||
        !prepare_growth_network(&second, 77U, 1U, 1U) ||
        minisnn_step(first) < 0 || minisnn_step(second) < 0 ||
        minisnn_connection_count(first) != 2U ||
        !connections_equal(first, second) ||
        !minisnn_get_structural_stats(first, &stats) ||
        stats.add_attempt_count != 1U || stats.add_success_count != 1U ||
        stats.rebuild_count != 1U || stats.current_connection_count != 2U ||
        stats.maximum_connection_count_observed != 2U ||
        minisnn_structural_event_count(first) != 1U ||
        !minisnn_get_structural_event(first, 0U, &event) ||
        event.type != MINISNN_TOPOLOGY_ADD || !event.applied)
        goto fail;
    for (size_t i = 0; i < minisnn_connection_count(first); i++)
    {
        if (!minisnn_get_structural_connection_state(first, i, &state))
            goto fail;
        if (state.growth_origin == 1U && state.birth_step == 1U)
            found_new_state = 1;
    }
    if (!found_new_state)
        goto fail;
    if (!prepare_growth_network(&batch, 88U, 6U, 2U) ||
        minisnn_step(batch) < 0 ||
        minisnn_connection_count(batch) != 3U ||
        !minisnn_get_structural_stats(batch, &stats) ||
        stats.add_success_count != 2U || stats.rebuild_count != 1U ||
        minisnn_structural_event_count(batch) != 2U)
        goto fail;
    minisnn_destroy(&first);
    minisnn_destroy(&second);
    minisnn_destroy(&batch);
    return 1;
fail:
    minisnn_destroy(&first);
    minisnn_destroy(&second);
    minisnn_destroy(&batch);
    return fail("candidate limit, growth determinism, or new state");
}

static int check_growth_threshold(void)
{
    MiniSNN *snn = minisnn_create(3);
    MiniSNNStructuralPlasticityConfig config = base_config();
    MiniSNNStructuralStats stats;
    config.pruning_enabled = 0;
    config.growth_enabled = 1;
    config.growth_score_threshold = 0.01;
    config.growth_candidate_count = 6U;
    if (snn == NULL || !minisnn_connect_delayed(snn, 0, 1, 2.0, 1) ||
        !minisnn_set_structural_plasticity_config(snn, &config) ||
        minisnn_step(snn) < 0 || minisnn_connection_count(snn) != 1U ||
        !minisnn_get_structural_stats(snn, &stats) ||
        stats.add_attempt_count != 0U || stats.rebuild_count != 0U)
        goto fail;
    minisnn_destroy(&snn);
    return 1;
fail:
    minisnn_destroy(&snn);
    return fail("growth threshold");
}

int main(void)
{
    if (!check_defaults_and_validation() ||
        !check_grace_pruning_and_reset() ||
        !check_growth_determinism_and_new_state() ||
        !check_growth_threshold())
        return 1;
    printf("Structural plasticity validation OK\n");
    return 0;
}

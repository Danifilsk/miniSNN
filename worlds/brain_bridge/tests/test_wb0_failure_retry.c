#include "minisnn_worlds_brain_bridge.h"
#include "../../domain/tests/wd0_test_support.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "WB0 retry test failed: %s at %d\n", #condition, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsBrainBridge *bridge;
    MiniSNN *brain_a;
    MiniSNN *brain_b;
    MiniSNNWorldsKernelEntityId actor_a;
    MiniSNNWorldsKernelEntityId actor_b;
} Scenario;

static int setup(Scenario *scenario)
{
    MiniSNNWorldsBrainBridgeConfig config = minisnn_worlds_brain_bridge_config_default();
    MiniSNNWorldsBrainBridgeError error;
    uint32_t channel;

    memset(scenario, 0, sizeof(*scenario));
    scenario->kernel = wd0_kernel();
    if (scenario->kernel == NULL ||
        !wd0_make_entity(scenario->kernel, 0, 0, &scenario->actor_a) ||
        !wd0_make_entity(scenario->kernel, 4000, 0, &scenario->actor_b))
    {
        return 0;
    }
    scenario->domain = wd0_domain_with_species(scenario->kernel);
    if (scenario->domain == NULL ||
        minisnn_worlds_domain_register_organism(
            scenario->domain, scenario->actor_a, UINT64_C(1), UINT64_C(50)) != 0 ||
        minisnn_worlds_domain_register_organism(
            scenario->domain, scenario->actor_b, UINT64_C(1), UINT64_C(50)) != 0)
    {
        return 0;
    }
    config.input_gain = 1000.0;
    config.core_steps_per_decision = 8U;
    for (channel = 0U; channel < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++channel)
    {
        config.action_neuron[channel] = (int)(6U + channel);
    }
    config.action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] = 0;
    scenario->bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    scenario->brain_a = minisnn_create(12);
    scenario->brain_b = minisnn_create(12);
    return scenario->bridge != NULL && scenario->brain_a != NULL && scenario->brain_b != NULL &&
           error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE &&
           minisnn_worlds_brain_bridge_bind(
               scenario->bridge, scenario->domain, scenario->actor_a, scenario->brain_a) ==
               MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE &&
           minisnn_worlds_brain_bridge_bind(
               scenario->bridge, scenario->domain, scenario->actor_b, scenario->brain_b) ==
               MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

static void destroy(Scenario *scenario)
{
    minisnn_worlds_brain_bridge_destroy(&scenario->bridge);
    minisnn_destroy(&scenario->brain_a);
    minisnn_destroy(&scenario->brain_b);
    minisnn_worlds_domain_destroy(scenario->domain);
    minisnn_worlds_kernel_destroy(scenario->kernel);
}

static int decide_pair(Scenario *scenario, MiniSNNWorldsDomainAction actions[2],
                       MiniSNNWorldsBrainDecisionReport reports[2])
{
    return minisnn_worlds_brain_bridge_decide(
               scenario->bridge, scenario->domain, scenario->actor_a,
               &actions[0], &reports[0]) == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE &&
           minisnn_worlds_brain_bridge_decide(
               scenario->bridge, scenario->domain, scenario->actor_b,
               &actions[1], &reports[1]) == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE;
}

int main(void)
{
    Scenario failed;
    Scenario clean;
    MiniSNNWorldsDomainAction failed_actions[2];
    MiniSNNWorldsDomainAction retry_actions[2];
    MiniSNNWorldsDomainAction clean_actions[2];
    MiniSNNWorldsBrainDecisionReport failed_reports[2];
    MiniSNNWorldsBrainDecisionReport retry_reports[2];
    MiniSNNWorldsBrainDecisionReport clean_reports[2];
    MiniSNNWorldsDomainActionResult results[2];
    uint64_t failed_domain_hash;
    uint64_t clean_domain_hash;
    uint64_t failed_kernel_hash;
    uint64_t clean_kernel_hash;
    MiniSNNWorldsTick failed_tick;
    int failed_a_steps;
    int failed_b_steps;

    CHECK(setup(&failed));
    CHECK(setup(&clean));
    CHECK(decide_pair(&failed, failed_actions, failed_reports));
    failed_a_steps = minisnn_current_step(failed.brain_a);
    failed_b_steps = minisnn_current_step(failed.brain_b);
    CHECK(failed_a_steps == 8 && failed_b_steps == 8);
    failed_tick = minisnn_worlds_domain_tick(failed.domain);

    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_domain_step(failed.domain, failed_actions, 2U, results) ==
          MINISNN_WORLDS_DOMAIN_ERROR_KERNEL_FAILURE);
    CHECK(minisnn_worlds_domain_tick(failed.domain) == failed_tick);
    CHECK(decide_pair(&failed, retry_actions, retry_reports));
    CHECK(retry_reports[0].cache_hit == 1U && retry_reports[1].cache_hit == 1U);
    CHECK(memcmp(failed_actions, retry_actions, sizeof(failed_actions)) == 0);
    CHECK(minisnn_current_step(failed.brain_a) == failed_a_steps);
    CHECK(minisnn_current_step(failed.brain_b) == failed_b_steps);

    minisnn_worlds_kernel_testing_fail_allocation_after(SIZE_MAX);
    CHECK(minisnn_worlds_domain_step(failed.domain, retry_actions, 2U, results) ==
          MINISNN_WORLDS_DOMAIN_ERROR_NONE);
    CHECK(decide_pair(&clean, clean_actions, clean_reports));
    CHECK(memcmp(clean_actions, failed_actions, sizeof(clean_actions)) == 0);
    CHECK(minisnn_worlds_domain_step(clean.domain, clean_actions, 2U, results) ==
          MINISNN_WORLDS_DOMAIN_ERROR_NONE);

    CHECK(minisnn_worlds_domain_state_hash(failed.domain, &failed_domain_hash) == 0);
    CHECK(minisnn_worlds_domain_state_hash(clean.domain, &clean_domain_hash) == 0);
    CHECK(minisnn_worlds_kernel_state_hash(failed.kernel, &failed_kernel_hash) == 0);
    CHECK(minisnn_worlds_kernel_state_hash(clean.kernel, &clean_kernel_hash) == 0);
    CHECK(failed_domain_hash == clean_domain_hash);
    CHECK(failed_kernel_hash == clean_kernel_hash);
    CHECK(minisnn_current_step(failed.brain_a) == minisnn_current_step(clean.brain_a));
    CHECK(minisnn_current_step(failed.brain_b) == minisnn_current_step(clean.brain_b));

    destroy(&failed);
    destroy(&clean);
    puts("WB0 failure retry preserves cached decisions and deterministic history OK");
    return 0;
}
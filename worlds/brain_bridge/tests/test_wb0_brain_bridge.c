#include "minisnn_worlds_brain_bridge.h"
#include "../../domain/tests/wd0_test_support.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "WB0 test failed: %s at %d\n", #condition, __LINE__); \
        return 1; \
    } \
} while (0)

static void configure_action_neurons(MiniSNNWorldsBrainBridgeConfig *config)
{
    uint32_t channel;
    for (channel = 0U; channel < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++channel)
    {
        config->action_neuron[channel] = (int)(6U + channel);
    }
    config->action_neuron[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] = 0;
}

static int advance_domain_ticks(MiniSNNWorldsDomain *domain,
                                MiniSNNWorldsKernelEntityId actor,
                                unsigned int count)
{
    MiniSNNWorldsDomainAction action = {0};
    MiniSNNWorldsDomainActionResult result;
    unsigned int index;

    action.actor = actor;
    action.type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    for (index = 0U; index < count; ++index)
    {
        if (minisnn_worlds_domain_step(domain, &action, 1U, &result) !=
            MINISNN_WORLDS_DOMAIN_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static int test_encoding_and_decoding(void)
{
    MiniSNNWorldsDomainPerception perception;
    MiniSNNWorldsBrainSensorFrameV1 frame;
    uint32_t scores[MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1] = {0};
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsBrainActionChannel selected;
    MiniSNNWorldsBrainBridgeFallback fallback;
    uint8_t tie;
    MiniSNNWorldsKernelEntityId actor = { UINT64_C(7) };
    uint32_t channel;

    memset(&perception, 0, sizeof(perception));
    perception.self_energy = UINT64_C(100);
    perception.self_hunger = UINT64_C(0);
    perception.nearest_food_present = true;
    perception.nearest_food_entity.value = UINT64_C(42);
    perception.nearest_food_delta_x = 2500;
    perception.nearest_food_delta_y = -500;
    perception.nearest_food_distance = UINT64_C(3000);
    CHECK(minisnn_worlds_brain_bridge_encode_perception_v1(
              &perception, UINT64_C(100), 1000.0, &frame) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(frame.values[0] == 1.0 && frame.values[1] == 0.0 && frame.values[2] == 1.0);
    CHECK(frame.values[3] == 1.0 && frame.values[4] == -0.5 && frame.values[5] == 1.0);

    perception.nearest_food_present = false;
    perception.nearest_food_delta_x = -999;
    perception.nearest_food_delta_y = 999;
    perception.nearest_food_distance = UINT64_C(999);
    CHECK(minisnn_worlds_brain_bridge_encode_perception_v1(
              &perception, UINT64_C(100), 0.5, &frame) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(frame.values[2] == 0.0 && frame.values[3] == 0.0 &&
          frame.values[4] == 0.0 && frame.values[5] == 0.0);

    perception.nearest_food_present = true;
    perception.nearest_food_entity.value = UINT64_C(42);
    for (channel = 0U; channel < MINISNN_WORLDS_BRAIN_BRIDGE_ACTION_COUNT_V1; ++channel)
    {
        memset(scores, 0, sizeof(scores));
        scores[channel] = UINT32_C(3);
        CHECK(minisnn_worlds_brain_bridge_decode_scores_v1(
                  &perception, scores, actor, 1000, &action, &selected, &tie, &fallback) ==
              MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
        CHECK(selected == (MiniSNNWorldsBrainActionChannel)channel);
        if (channel == MINISNN_WORLDS_BRAIN_ACTION_EAT)
        {
            CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
                  action.eat_target.value == perception.nearest_food_entity.value);
        }
        else if (channel == MINISNN_WORLDS_BRAIN_ACTION_WAIT)
        {
            CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT);
        }
        else
        {
            CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_MOVE);
        }
    }

    memset(scores, 0, sizeof(scores));
    CHECK(minisnn_worlds_brain_bridge_decode_scores_v1(
              &perception, scores, actor, 1000, &action, &selected, &tie, &fallback) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT &&
          fallback == MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_NO_OUTPUT);

    memset(scores, 0, sizeof(scores));
    scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] = UINT32_C(4);
    scores[MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X] = UINT32_C(4);
    CHECK(minisnn_worlds_brain_bridge_decode_scores_v1(
              &perception, scores, actor, 1000, &action, &selected, &tie, &fallback) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(selected == MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X && tie == 1U);

    memset(scores, 0, sizeof(scores));
    scores[MINISNN_WORLDS_BRAIN_ACTION_EAT] = UINT32_C(1);
    perception.nearest_food_present = false;
    CHECK(minisnn_worlds_brain_bridge_decode_scores_v1(
              &perception, scores, actor, 1000, &action, &selected, &tie, &fallback) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_WAIT &&
          fallback == MINISNN_WORLDS_BRAIN_BRIDGE_FALLBACK_EAT_WITHOUT_FOOD);
    return 0;
}

static int test_config_and_binding_errors(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId organism;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsKernelEntityId unknown = { UINT64_C(9999) };
    MiniSNNWorldsBrainBridgeConfig config = minisnn_worlds_brain_bridge_config_default();
    MiniSNNWorldsBrainBridgeError error;
    MiniSNNWorldsBrainBridge *bridge;
    MiniSNN *brain;

    CHECK(kernel != NULL);
    CHECK(wd0_make_entity(kernel, 0, 0, &organism));
    CHECK(wd0_make_entity(kernel, 1000, 0, &food));
    domain = wd0_domain_with_species(kernel);
    CHECK(domain != NULL);
    CHECK(minisnn_worlds_domain_register_organism(domain, organism, UINT64_C(1), UINT64_C(50)) == 0);
    CHECK(minisnn_worlds_domain_register_food(domain, food, UINT64_C(1)) == 0);

    config.core_steps_per_decision = 0U;
    CHECK(!minisnn_worlds_brain_bridge_config_is_valid(&config));
    bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    CHECK(bridge == NULL && error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT);

    config = minisnn_worlds_brain_bridge_config_default();
    configure_action_neurons(&config);
    bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    brain = minisnn_create(12);
    CHECK(bridge != NULL && brain != NULL && error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, unknown, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_UNKNOWN_ACTOR);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, food, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_ORGANISM);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, organism, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, organism, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DUPLICATE_BINDING);
    CHECK(minisnn_worlds_brain_bridge_unbind(bridge, unknown) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NOT_BOUND);
    CHECK(minisnn_worlds_brain_bridge_unbind(bridge, organism) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_binding_count(bridge) == 0U);

    config.sensor_neuron[0] = 99;
    minisnn_worlds_brain_bridge_destroy(&bridge);
    bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    CHECK(bridge != NULL && error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, organism, brain) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    {
        MiniSNNWorldsDomainAction action = {0};
        MiniSNNWorldsBrainDecisionReport report;
        MiniSNNWorldsBrainDecisionReport expected_report;
        int before = minisnn_current_step(brain);
        memset(&report, 0xA5, sizeof(report));
        expected_report = report;
        action.type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
        CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, organism, &action, &report) ==
              MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_INVALID_ARGUMENT);
        CHECK(minisnn_current_step(brain) == before);
        CHECK(action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT);
        CHECK(memcmp(&report, &expected_report, sizeof(report)) == 0);
    }

    minisnn_worlds_brain_bridge_destroy(&bridge);
    minisnn_destroy(&brain);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_core_cache_and_multi_organism(void)
{
    MiniSNNWorldsKernel *kernel = wd0_kernel();
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor_a;
    MiniSNNWorldsKernelEntityId actor_b;
    MiniSNNWorldsKernelEntityId food;
    MiniSNNWorldsBrainBridgeConfig config = minisnn_worlds_brain_bridge_config_default();
    MiniSNNWorldsBrainBridgeError error;
    MiniSNNWorldsBrainBridge *bridge;
    MiniSNN *brain_a;
    MiniSNN *brain_b;
    MiniSNNWorldsDomainAction action_a;
    MiniSNNWorldsDomainAction action_b;
    MiniSNNWorldsBrainDecisionReport report_a;
    MiniSNNWorldsBrainDecisionReport report_b;
    MiniSNNWorldsDomainActionResult results[2];
    int a_before;
    int b_before;

    CHECK(kernel != NULL);
    CHECK(wd0_make_entity(kernel, 0, 0, &actor_a));
    CHECK(wd0_make_entity(kernel, 4000, 0, &actor_b));
    CHECK(wd0_make_entity(kernel, 2000, 0, &food));
    domain = wd0_domain_with_species(kernel);
    CHECK(domain != NULL);
    CHECK(minisnn_worlds_domain_register_organism(domain, actor_a, UINT64_C(1), UINT64_C(50)) == 0);
    CHECK(minisnn_worlds_domain_register_organism(domain, actor_b, UINT64_C(1), UINT64_C(20)) == 0);
    CHECK(minisnn_worlds_domain_register_food(domain, food, UINT64_C(10)) == 0);

    config.input_gain = 1000.0;
    config.core_steps_per_decision = 8U;
    configure_action_neurons(&config);
    bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    brain_a = minisnn_create(12);
    brain_b = minisnn_create(12);
    CHECK(bridge != NULL && brain_a != NULL && brain_b != NULL && error == 0);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, actor_a, brain_a) == 0);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain, actor_b, brain_b) == 0);

    a_before = minisnn_current_step(brain_a);
    b_before = minisnn_current_step(brain_b);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, actor_a, &action_a, &report_a) == 0);
    CHECK(minisnn_current_step(brain_a) == a_before + 8);
    CHECK(minisnn_current_step(brain_b) == b_before);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, actor_b, &action_b, &report_b) == 0);
    CHECK(minisnn_current_step(brain_a) == a_before + 8);
    CHECK(minisnn_current_step(brain_b) == b_before + 8);
    CHECK(report_a.actor.value == actor_a.value && report_b.actor.value == actor_b.value);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, actor_a, &action_a, &report_a) == 0);
    CHECK(report_a.cache_hit == 1U && minisnn_current_step(brain_a) == a_before + 8);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, actor_b, &action_b, &report_b) == 0);
    CHECK(report_b.cache_hit == 1U && minisnn_current_step(brain_b) == b_before + 8);

    CHECK(minisnn_worlds_domain_step(domain, (MiniSNNWorldsDomainAction[]){action_a, action_b},
                                     2U, results) == 0);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain, actor_a, &action_a, &report_a) == 0);
    CHECK(report_a.cache_hit == 0U && minisnn_current_step(brain_a) == a_before + 16);

    minisnn_worlds_brain_bridge_destroy(&bridge);
    minisnn_destroy(&brain_a);
    minisnn_destroy(&brain_b);
    minisnn_worlds_domain_destroy(domain);
    minisnn_worlds_kernel_destroy(kernel);
    return 0;
}

static int test_cross_domain_isolation(void)
{
    MiniSNNWorldsKernel *kernel_a = wd0_kernel();
    MiniSNNWorldsKernel *kernel_b = wd0_kernel();
    MiniSNNWorldsDomain *domain_a;
    MiniSNNWorldsDomain *domain_b;
    MiniSNNWorldsKernelEntityId actor_a;
    MiniSNNWorldsKernelEntityId actor_b;
    MiniSNNWorldsBrainBridgeConfig config = minisnn_worlds_brain_bridge_config_default();
    MiniSNNWorldsBrainBridgeError error;
    MiniSNNWorldsBrainBridge *bridge;
    MiniSNN *brain_a;
    MiniSNN *brain_b;
    MiniSNNWorldsDomainAction action_a;
    MiniSNNWorldsDomainAction cached_action;
    MiniSNNWorldsDomainAction mismatch_action;
    MiniSNNWorldsDomainAction expected_mismatch_action;
    MiniSNNWorldsBrainDecisionReport report_a;
    MiniSNNWorldsBrainDecisionReport cached_report;
    MiniSNNWorldsBrainDecisionReport mismatch_report;
    MiniSNNWorldsBrainDecisionReport expected_mismatch_report;
    MiniSNNWorldsBrainBridgeDiagnostics diagnostics_before;
    MiniSNNWorldsBrainBridgeDiagnostics diagnostics_after;
    int a_before;
    int b_before;

    CHECK(kernel_a != NULL && kernel_b != NULL);
    CHECK(wd0_make_entity(kernel_a, 0, 0, &actor_a));
    CHECK(wd0_make_entity(kernel_b, 0, 0, &actor_b));
    CHECK(actor_a.value == actor_b.value);
    domain_a = wd0_domain_with_species(kernel_a);
    domain_b = wd0_domain_with_species(kernel_b);
    CHECK(domain_a != NULL && domain_b != NULL);
    CHECK(minisnn_worlds_domain_register_organism(domain_a, actor_a, UINT64_C(1), UINT64_C(50)) == 0);
    CHECK(minisnn_worlds_domain_register_organism(domain_b, actor_b, UINT64_C(1), UINT64_C(50)) == 0);
    CHECK(advance_domain_ticks(domain_a, actor_a, 2U));
    CHECK(advance_domain_ticks(domain_b, actor_b, 2U));
    CHECK(minisnn_worlds_domain_tick(domain_a) == minisnn_worlds_domain_tick(domain_b));

    config.input_gain = 1000.0;
    configure_action_neurons(&config);
    bridge = minisnn_worlds_brain_bridge_create(&config, &error);
    brain_a = minisnn_create(12);
    brain_b = minisnn_create(12);
    CHECK(bridge != NULL && brain_a != NULL && brain_b != NULL &&
          error == MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain_a, actor_a, brain_a) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain_b, actor_b, brain_b) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH);
    CHECK(minisnn_worlds_brain_bridge_binding_count(bridge) == 1U);

    a_before = minisnn_current_step(brain_a);
    b_before = minisnn_current_step(brain_b);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain_a, actor_a, &action_a, &report_a) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(report_a.cache_hit == 0U && minisnn_current_step(brain_a) == a_before + 8);
    CHECK(minisnn_worlds_brain_bridge_get_diagnostics(bridge, &diagnostics_before) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);

    memset(&mismatch_action, 0x5A, sizeof(mismatch_action));
    expected_mismatch_action = mismatch_action;
    memset(&mismatch_report, 0xA5, sizeof(mismatch_report));
    expected_mismatch_report = mismatch_report;
    CHECK(minisnn_worlds_brain_bridge_decide(
              bridge, domain_b, actor_b, &mismatch_action, &mismatch_report) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH);
    CHECK(minisnn_worlds_brain_bridge_last_error(bridge) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_DOMAIN_MISMATCH);
    CHECK(minisnn_current_step(brain_a) == a_before + 8);
    CHECK(minisnn_current_step(brain_b) == b_before);
    CHECK(memcmp(&mismatch_action, &expected_mismatch_action, sizeof(mismatch_action)) == 0);
    CHECK(memcmp(&mismatch_report, &expected_mismatch_report, sizeof(mismatch_report)) == 0);
    CHECK(minisnn_worlds_brain_bridge_get_diagnostics(bridge, &diagnostics_after) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(memcmp(&diagnostics_before, &diagnostics_after, sizeof(diagnostics_before)) == 0);

    cached_action = action_a;
    cached_report = report_a;
    cached_report.cache_hit = 1U;
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain_a, actor_a, &action_a, &report_a) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(memcmp(&action_a, &cached_action, sizeof(action_a)) == 0);
    CHECK(memcmp(&report_a, &cached_report, sizeof(report_a)) == 0);
    CHECK(minisnn_current_step(brain_a) == a_before + 8);

    CHECK(minisnn_worlds_brain_bridge_unbind(bridge, actor_a) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_binding_count(bridge) == 0U);
    CHECK(minisnn_worlds_brain_bridge_bind(bridge, domain_b, actor_b, brain_b) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(minisnn_worlds_brain_bridge_decide(bridge, domain_b, actor_b, &action_a, &report_a) ==
          MINISNN_WORLDS_BRAIN_BRIDGE_ERROR_NONE);
    CHECK(report_a.cache_hit == 0U && minisnn_current_step(brain_b) == b_before + 8);

    minisnn_worlds_brain_bridge_destroy(&bridge);
    minisnn_destroy(&brain_a);
    minisnn_destroy(&brain_b);
    minisnn_worlds_domain_destroy(domain_a);
    minisnn_worlds_domain_destroy(domain_b);
    minisnn_worlds_kernel_destroy(kernel_a);
    minisnn_worlds_kernel_destroy(kernel_b);
    return 0;
}

int main(void)
{
    if (test_encoding_and_decoding() != 0 ||
        test_config_and_binding_errors() != 0 ||
        test_core_cache_and_multi_organism() != 0 ||
        test_cross_domain_isolation() != 0)
    {
        return 1;
    }
    puts("WB0 Brain Bridge encoder, decoder, binding, cache and cross-domain isolation OK");
    return 0;
}
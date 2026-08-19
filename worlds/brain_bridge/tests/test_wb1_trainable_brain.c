#include "minisnn_worlds_brain_bridge.h"
#include "../../domain/tests/wd0_test_support.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef MINISNN_TESTING
void minisnn_test_wb1_fail_after_checkpoint(int enabled);
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "WB1 test failed: %s at %d\n", #condition, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
} TestWorld;

static int test_world_create_at(
    TestWorld *world,
    MiniSNNWorldsKernelScalar food_x,
    MiniSNNWorldsKernelScalar food_y)
{
    memset(world, 0, sizeof(*world));
    world->kernel = wd0_kernel();
    if (world->kernel == NULL ||
        !wd0_make_entity(world->kernel, 0, 0, &world->actor) ||
        !wd0_make_entity(world->kernel, food_x, food_y, &world->food))
        return 0;
    world->domain = wd0_domain_with_species(world->kernel);
    return world->domain != NULL &&
        minisnn_worlds_domain_register_organism(
            world->domain, world->actor, UINT64_C(1), UINT64_C(50)) ==
            MINISNN_WORLDS_DOMAIN_ERROR_NONE &&
        minisnn_worlds_domain_register_food(world->domain, world->food,
                                             UINT64_C(15)) ==
            MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}

static int test_world_create(TestWorld *world)
{
    return test_world_create_at(world, 1000, 0);
}

static void test_world_destroy(TestWorld *world)
{
    if (world == NULL)
        return;
    minisnn_worlds_domain_destroy(world->domain);
    minisnn_worlds_kernel_destroy(world->kernel);
    memset(world, 0, sizeof(*world));
}

static unsigned long test_process_id(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static int test_brain_filename(char *out_path, size_t out_size, const char *role)
{
    int written;

    if (out_path == NULL || out_size == 0U || role == NULL)
        return 0;
    written = snprintf(out_path, out_size, "wb1_%lu_%s.brain",
                       test_process_id(), role);
    return written >= 0 && (size_t)written < out_size;
}

static MiniSNNWorldsDomainActionResult zero_result(void)
{
    MiniSNNWorldsDomainActionResult result;
    memset(&result, 0, sizeof(result));
    result.status = MINISNN_WORLDS_DOMAIN_ACTION_APPLIED;
    result.reason = MINISNN_WORLDS_DOMAIN_ACTION_REASON_NONE;
    result.energy_before = UINT64_C(50);
    result.energy_after = UINT64_C(50);
    return result;
}

static int test_feedback_once_and_episode_rebind(void)
{
    MiniSNNWorldsBrainConfig config = minisnn_worlds_trainable_brain_config_default();
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    MiniSNNWorldsTrainableBrainReport first;
    MiniSNNWorldsTrainableBrainReport cached;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result = zero_result();
    MiniSNNWorldsKernelEntityId wrong_actor = { UINT64_C(999) };
    TestWorld first_world;
    TestWorld second_world;
    uint64_t before;
    int core_step;

    CHECK(test_world_create(&first_world));
    CHECK(test_world_create(&second_world));
    brain = minisnn_worlds_trainable_brain_create(&config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_trainable_brain_bind(brain, first_world.domain,
                                              first_world.actor));
    CHECK(!minisnn_worlds_trainable_brain_bind(brain, second_world.domain,
                                               second_world.actor));
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_DOMAIN_MISMATCH);
    CHECK(minisnn_worlds_trainable_brain_decide(brain, first_world.domain,
                                                first_world.actor, &action, &first));
    core_step = minisnn_worlds_trainable_brain_core_step(brain);
    CHECK(minisnn_worlds_trainable_brain_decide(brain, first_world.domain,
                                                first_world.actor, &action, &cached));
    CHECK(cached.cache_hit == 1U && cached.core_tick == first.core_tick);
    CHECK(!minisnn_worlds_trainable_brain_apply_action_result(
        brain, first_world.domain, wrong_actor, first.domain_tick, &action, &result));
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_MISMATCH);
    CHECK(minisnn_worlds_trainable_brain_core_step(brain) == core_step);
    CHECK(!minisnn_worlds_trainable_brain_apply_action_result(
        brain, first_world.domain, first_world.actor, first.domain_tick + 1U,
        &action, &result));
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_MISMATCH);
    CHECK(minisnn_worlds_trainable_brain_apply_action_result(
        brain, first_world.domain, first_world.actor, first.domain_tick,
        &action, &result));
    CHECK(minisnn_worlds_trainable_brain_last_reward(brain) == 0.0);
    CHECK(!minisnn_worlds_trainable_brain_apply_action_result(
        brain, first_world.domain, first_world.actor, first.domain_tick,
        &action, &result));
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_FEEDBACK_NOT_PENDING);
    before = minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(minisnn_worlds_trainable_brain_reset_episode(brain));
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) == before);
    CHECK(minisnn_worlds_trainable_brain_bind(brain, second_world.domain,
                                              second_world.actor));
    CHECK(minisnn_worlds_trainable_brain_decide(brain, second_world.domain,
                                                second_world.actor, &action, &first));
    CHECK(minisnn_worlds_trainable_brain_apply_action_result(
        brain, second_world.domain, second_world.actor, first.domain_tick,
        &action, &result));
    minisnn_worlds_trainable_brain_destroy(&brain);
    test_world_destroy(&second_world);
    test_world_destroy(&first_world);
    return 0;
}

static int rewrite_signature(const char *path, const char *key)
{
    char buffer[4096];
    FILE *file;
    char *found;

    file = fopen(path, "rb");
    if (file == NULL || fread(buffer, 1U, sizeof(buffer) - 1U, file) == 0U)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    buffer[ftell(file)] = '\0';
    fclose(file);
    found = strstr(buffer, key);
    if (found == NULL || found[strlen(key)] == '\0')
        return 0;
    found[strlen(key)] = found[strlen(key)] == '0' ? '1' : '0';
    file = fopen(path, "wb");
    if (file == NULL)
        return 0;
    if (fwrite(buffer, 1U, strlen(buffer), file) != strlen(buffer))
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int test_config_manifest_and_transaction(void)
{
    MiniSNNWorldsBrainConfig config = minisnn_worlds_trainable_brain_config_default();
    MiniSNNWorldsBrainConfig loaded_config;
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    MiniSNNWorldsTrainableBrain *loaded;
    uint64_t signature;
    char before[4096];
    char after[4096];
    FILE *file;
    size_t bytes;
    char manifest_path[96];
    char action_path[96];
    char transaction_path[96];

    CHECK(test_brain_filename(manifest_path, sizeof(manifest_path), "manifest"));
    CHECK(test_brain_filename(action_path, sizeof(action_path), "action"));
    CHECK(test_brain_filename(transaction_path, sizeof(transaction_path), "transaction"));
    config.neural_config.dt = 0.25;
    config.neural_config.tau = 18.0;
    config.neural_config.v_rest = -62.0;
    config.neural_config.v_reset = -63.0;
    config.neural_config.v_threshold = -47.0;
    config.neural_config.resistance = 1.25;
    config.neural_config.synaptic_decay = 0.90;
    config.neuron_count = 24U;
    config.neural_config.neuron_count = 24;
    config.action_population_size = 2U;
    config.inhibitory_count = 3U;
    config.connection_probability = 0.35;
    config.small_world_neighbors = 2U;
    config.small_world_rewire_probability = 0.20;
    config.excitatory_weight = 125.0;
    config.inhibitory_weight = -80.0;
    config.allow_inhibitory_to_inhibitory = 0;
    CHECK(minisnn_worlds_trainable_brain_config_is_valid(&config));
    brain = minisnn_worlds_trainable_brain_create(&config, &error);
    CHECK(brain != NULL);
    signature = minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(minisnn_worlds_trainable_brain_save(brain, manifest_path, &error));
    loaded = minisnn_worlds_trainable_brain_load(manifest_path, &error);
    CHECK(loaded != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_trainable_brain_get_config(loaded, &loaded_config));
    CHECK(loaded_config.neural_config.dt == config.neural_config.dt &&
          loaded_config.neural_config.tau == config.neural_config.tau &&
          loaded_config.neural_config.v_rest == config.neural_config.v_rest &&
          loaded_config.neural_config.synaptic_decay == config.neural_config.synaptic_decay &&
          loaded_config.action_population_size == config.action_population_size &&
          loaded_config.inhibitory_count == config.inhibitory_count &&
          loaded_config.connection_probability == config.connection_probability &&
          loaded_config.small_world_neighbors == config.small_world_neighbors &&
          loaded_config.small_world_rewire_probability ==
              config.small_world_rewire_probability &&
          loaded_config.excitatory_weight == config.excitatory_weight &&
          loaded_config.inhibitory_weight == config.inhibitory_weight &&
          loaded_config.allow_inhibitory_to_inhibitory ==
              config.allow_inhibitory_to_inhibitory);
    CHECK(minisnn_worlds_trainable_brain_weight_signature(loaded) == signature);
    minisnn_worlds_trainable_brain_destroy(&loaded);
    CHECK(rewrite_signature(manifest_path, "sensor_schema_signature="));
    loaded = minisnn_worlds_trainable_brain_load(manifest_path, &error);
    CHECK(loaded == NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INCOMPATIBLE);
    CHECK(minisnn_worlds_trainable_brain_save(brain, action_path, &error));
    CHECK(rewrite_signature(action_path, "action_schema_signature="));
    loaded = minisnn_worlds_trainable_brain_load(action_path, &error);
    CHECK(loaded == NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_INCOMPATIBLE);
    CHECK(minisnn_worlds_trainable_brain_save(brain, transaction_path, &error));
    file = fopen(transaction_path, "rb");
    CHECK(file != NULL);
    bytes = fread(before, 1U, sizeof(before), file);
    fclose(file);
#ifdef MINISNN_TESTING
    minisnn_test_wb1_fail_after_checkpoint(1);
    CHECK(!minisnn_worlds_trainable_brain_save(brain, transaction_path, &error));
    minisnn_test_wb1_fail_after_checkpoint(0);
#endif
    file = fopen(transaction_path, "rb");
    CHECK(file != NULL && fread(after, 1U, sizeof(after), file) == bytes);
    fclose(file);
    CHECK(memcmp(before, after, bytes) == 0);
    CHECK(minisnn_worlds_trainable_brain_set_mode(
        brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION));
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) == signature);
    CHECK(minisnn_worlds_trainable_brain_set_mode(
        brain, MINISNN_WORLDS_BRAIN_MODE_TRAINING));
    minisnn_worlds_trainable_brain_destroy(&brain);
    return 0;
}

static int test_factory_has_no_duplicates(void)
{
    MiniSNNTopologyFactoryConfig config = minisnn_topology_factory_default();
    MiniSNN *invalid = NULL;
    MiniSNN *network = NULL;
    MiniSNN *same_seed = NULL;
    uint64_t seed;
    int passed = 0;

#define FACTORY_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "WB1 factory test failed: %s at %d\n", #condition, __LINE__); \
        goto cleanup; \
    } \
} while (0)

    invalid = minisnn_create(16);
    config.kind = MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD;
    config.small_world_neighbors = 1U;
    FACTORY_CHECK(invalid != NULL);
    FACTORY_CHECK(!minisnn_topology_factory_build(invalid, &config));
    minisnn_destroy(&invalid);

    for (seed = 1U; seed <= 8U; ++seed)
    {
        unsigned char seen[16U * 16U] = {0U};
        uint64_t signature;
        uint64_t same_signature;
        size_t index;

        network = minisnn_create(16);
        same_seed = minisnn_create(16);
        FACTORY_CHECK(network != NULL && same_seed != NULL);
        config.seed = seed;
        config.kind = seed % 3U == 0U ?
            MINISNN_TOPOLOGY_FACTORY_FULLY_CONNECTED :
            (seed & 1U) ? MINISNN_TOPOLOGY_FACTORY_RANDOM :
                            MINISNN_TOPOLOGY_FACTORY_SMALL_WORLD;
        config.small_world_neighbors = 4U;
        config.inhibitory_count = 4U;
        FACTORY_CHECK(minisnn_topology_factory_build(network, &config));
        FACTORY_CHECK(minisnn_topology_factory_build(same_seed, &config));
        FACTORY_CHECK(minisnn_get_topology_signature(network, &signature));
        FACTORY_CHECK(minisnn_get_topology_signature(same_seed, &same_signature));
        FACTORY_CHECK(signature == same_signature);
        for (index = 0U; index < minisnn_connection_count(network); ++index)
        {
            MiniSNNConnectionInfo connection;
            size_t key;

            FACTORY_CHECK(minisnn_get_connection(network, index, &connection));
            FACTORY_CHECK(connection.source != connection.target);
            key = connection.source * 16U + connection.target;
            FACTORY_CHECK(seen[key] == 0U);
            seen[key] = 1U;
        }
        minisnn_destroy(&network);
        minisnn_destroy(&same_seed);
    }
    passed = 1;

cleanup:
    minisnn_destroy(&same_seed);
    minisnn_destroy(&network);
    minisnn_destroy(&invalid);
#undef FACTORY_CHECK
    return passed ? 0 : 1;
}

typedef struct
{
    uint64_t topology_signature;
    uint64_t config_signature;
    uint64_t initial_signature;
    uint64_t learned_signature;
    uint64_t reset_signature;
    uint64_t evaluation_signature;
    uint64_t loaded_signature;
    int core_step;
} WB1Summary;

static WB1Summary g_summary;

static int apply_real_action(
    MiniSNNWorldsTrainableBrain *brain,
    TestWorld *world,
    const MiniSNNWorldsTrainableBrainReport *report,
    const MiniSNNWorldsDomainAction *action,
    MiniSNNWorldsDomainActionResult *out_result)
{
    if (minisnn_worlds_domain_step(
            world->domain, action, 1U, out_result) !=
        MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        return 0;
    }
    return minisnn_worlds_trainable_brain_apply_action_result(
        brain, world->domain, world->actor, report->domain_tick,
        action, out_result);
}

static int find_positive_training_case(
    MiniSNNWorldsTrainableBrain **out_brain,
    TestWorld *out_world,
    MiniSNNWorldsBrainConfig *out_config,
    MiniSNNWorldsDomainAction *out_action,
    MiniSNNWorldsTrainableBrainReport *out_report)
{
    uint64_t seed;

    if (out_brain == NULL || out_world == NULL || out_config == NULL ||
        out_action == NULL || out_report == NULL)
    {
        return 0;
    }
    memset(out_world, 0, sizeof(*out_world));
    *out_brain = NULL;
    for (seed = 1U; seed <= UINT64_C(256); ++seed)
    {
        MiniSNNWorldsBrainConfig config =
            minisnn_worlds_trainable_brain_config_default();
        MiniSNNWorldsTrainableBrainError error;
        MiniSNNWorldsTrainableBrain *brain;
        MiniSNNWorldsDomainAction action;
        MiniSNNWorldsTrainableBrainReport report;
        TestWorld world;

        config.topology = MINISNN_WORLDS_BRAIN_TOPOLOGY_RANDOM;
        config.connection_probability = 0.40;
        config.seed = seed;
        config.decision_steps_per_tick = 16U;
        config.excitatory_weight = 200.0;
        config.inhibitory_weight = -100.0;
        if (!test_world_create_at(&world, 0, 0))
            return 0;
        brain = minisnn_worlds_trainable_brain_create(&config, &error);
        if (brain != NULL &&
            minisnn_worlds_trainable_brain_bind(brain, world.domain, world.actor) &&
            minisnn_worlds_trainable_brain_decide(
                brain, world.domain, world.actor, &action, &report) &&
            action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT)
        {
            *out_brain = brain;
            *out_world = world;
            *out_config = config;
            *out_action = action;
            *out_report = report;
            return 1;
        }
        minisnn_worlds_trainable_brain_destroy(&brain);
        test_world_destroy(&world);
    }
    return 0;
}

static int test_training_evaluation_boundary_and_save(void)
{
    MiniSNNWorldsTrainableBrain *brain;
    MiniSNNWorldsBrainConfig config;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsTrainableBrainReport report;
    MiniSNNWorldsDomainActionResult result;
    MiniSNNWorldsTrainableBrainError error;
    TestWorld world;
    uint64_t before_eval;
    int passed = 0;
    char pending_path[96];

    CHECK(test_brain_filename(pending_path, sizeof(pending_path), "pending"));
    remove(pending_path);
    CHECK(find_positive_training_case(
        &brain, &world, &config, &action, &report));
    g_summary.topology_signature =
        minisnn_worlds_trainable_brain_topology_signature(brain);
    g_summary.config_signature =
        minisnn_worlds_trainable_brain_config_signature(brain);
    g_summary.initial_signature =
        minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(!minisnn_worlds_trainable_brain_save(
        brain, pending_path, &error));
    CHECK(error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT);
    CHECK(fopen(pending_path, "rb") == NULL);
    CHECK(apply_real_action(brain, &world, &report, &action, &result));
    CHECK(result.status == MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
          result.energy_after > result.energy_before);
    CHECK(minisnn_worlds_trainable_brain_last_reward(brain) ==
          config.reward_profile.eat_applied_reward);
    CHECK(!minisnn_worlds_trainable_brain_save(
        brain, pending_path, &error));
    CHECK(error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_PENDING_ACTION_RESULT);
    CHECK(fopen(pending_path, "rb") == NULL);
    CHECK(minisnn_worlds_trainable_brain_set_mode(
        brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION));
    g_summary.learned_signature =
        minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(g_summary.learned_signature != g_summary.initial_signature);
    CHECK(minisnn_worlds_trainable_brain_set_mode(
        brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION));
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) ==
          g_summary.learned_signature);
    before_eval = g_summary.learned_signature;
    CHECK(minisnn_worlds_trainable_brain_decide(
        brain, world.domain, world.actor, &action, &report));
    CHECK(apply_real_action(brain, &world, &report, &action, &result));
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) == before_eval);
    g_summary.evaluation_signature = before_eval;
    CHECK(minisnn_worlds_trainable_brain_set_mode(
        brain, MINISNN_WORLDS_BRAIN_MODE_TRAINING));
    CHECK(minisnn_worlds_trainable_brain_reset_episode(brain));
    g_summary.reset_signature =
        minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(g_summary.reset_signature == before_eval);
    {
        TestWorld resumed_world;
        MiniSNNWorldsTrainableBrainReport resumed_report;
        MiniSNNWorldsDomainAction resumed_action;

        CHECK(test_world_create_at(&resumed_world, 0, 0));
        CHECK(minisnn_worlds_trainable_brain_bind(
            brain, resumed_world.domain, resumed_world.actor));
        CHECK(minisnn_worlds_trainable_brain_decide(
            brain, resumed_world.domain, resumed_world.actor,
            &resumed_action, &resumed_report));
        CHECK(resumed_action.type == MINISNN_WORLDS_DOMAIN_ACTION_EAT);
        CHECK(apply_real_action(
            brain, &resumed_world, &resumed_report, &resumed_action, &result));
        CHECK(minisnn_worlds_trainable_brain_set_mode(
            brain, MINISNN_WORLDS_BRAIN_MODE_EVALUATION));
        CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) != before_eval);
        CHECK(minisnn_worlds_trainable_brain_reset_episode(brain));
        test_world_destroy(&resumed_world);
    }
    g_summary.core_step = minisnn_worlds_trainable_brain_core_step(brain);
    {
        uint64_t saved_signature =
            minisnn_worlds_trainable_brain_weight_signature(brain);
        CHECK(minisnn_worlds_trainable_brain_save(brain, pending_path, &error));
        {
            MiniSNNWorldsTrainableBrain *loaded =
                minisnn_worlds_trainable_brain_load(pending_path, &error);
            CHECK(loaded != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
            g_summary.loaded_signature =
                minisnn_worlds_trainable_brain_weight_signature(loaded);
            CHECK(g_summary.loaded_signature == saved_signature);
            minisnn_worlds_trainable_brain_destroy(&loaded);
        }
    }
    passed = 1;
    minisnn_worlds_trainable_brain_destroy(&brain);
    test_world_destroy(&world);
    remove(pending_path);
    return passed ? 0 : 1;
}

static int test_terminal_feedback_exactly_once(void)
{
    MiniSNNWorldsBrainConfig config =
        minisnn_worlds_trainable_brain_config_default();
    MiniSNNWorldsTrainableBrainError error;
    MiniSNNWorldsTrainableBrain *brain;
    MiniSNNWorldsTrainableBrainReport report;
    MiniSNNWorldsDomainAction action;
    MiniSNNWorldsDomainActionResult result;
    TestWorld world;
    uint64_t after_terminal;

    CHECK(test_world_create(&world));
    brain = minisnn_worlds_trainable_brain_create(&config, &error);
    CHECK(brain != NULL && error == MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_NONE);
    CHECK(minisnn_worlds_trainable_brain_bind(brain, world.domain, world.actor));
    CHECK(minisnn_worlds_trainable_brain_decide(
        brain, world.domain, world.actor, &action, &report));
    CHECK(apply_real_action(brain, &world, &report, &action, &result));
    CHECK(minisnn_worlds_trainable_brain_apply_terminal_feedback(brain, -1.0));
    after_terminal = minisnn_worlds_trainable_brain_weight_signature(brain);
    CHECK(!minisnn_worlds_trainable_brain_apply_terminal_feedback(brain, -1.0));
    CHECK(minisnn_worlds_trainable_brain_last_error(brain) ==
          MINISNN_WORLDS_TRAINABLE_BRAIN_ERROR_CORE_FAILURE);
    CHECK(minisnn_worlds_trainable_brain_weight_signature(brain) == after_terminal);
    CHECK(minisnn_worlds_trainable_brain_reset_episode(brain));
    minisnn_worlds_trainable_brain_destroy(&brain);
    test_world_destroy(&world);
    return 0;
}

int main(void)
{
    if (test_feedback_once_and_episode_rebind() != 0 ||
        test_config_manifest_and_transaction() != 0 ||
        test_factory_has_no_duplicates() != 0 ||
        test_training_evaluation_boundary_and_save() != 0 ||
        test_terminal_feedback_exactly_once() != 0)
        return 1;
    printf("topology_signature=%016llx\nconfig_signature=%016llx\n"
           "initial_weight_signature=%016llx\nlearned_weight_signature=%016llx\n"
           "post_episode_reset_weight_signature=%016llx\n"
           "evaluation_weight_signature=%016llx\nloaded_weight_signature=%016llx\n"
           "core_tick=%d\n",
           (unsigned long long)g_summary.topology_signature,
           (unsigned long long)g_summary.config_signature,
           (unsigned long long)g_summary.initial_signature,
           (unsigned long long)g_summary.learned_signature,
           (unsigned long long)g_summary.reset_signature,
           (unsigned long long)g_summary.evaluation_signature,
           (unsigned long long)g_summary.loaded_signature,
           g_summary.core_step);
    puts("WB1 trainable brain hardening OK");
    return 0;
}
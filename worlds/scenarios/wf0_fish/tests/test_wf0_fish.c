#include "wf0_fish.h"

#include <inttypes.h>
#include <stdio.h>

static int assert_true(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "WF0 test failure: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_neural_direction_and_metabolism(void)
{
    WF0FishWorld *right = wf0_fish_world_create(1000, 0);
    WF0FishWorld *left = wf0_fish_world_create(-1000, 0);
    WF0FishTickRecord right_first;
    WF0FishTickRecord right_second;
    WF0FishTickRecord left_first;
    int passed = 0;

    if (!assert_true(right != NULL && left != NULL, "world creation"))
    {
        goto done;
    }
    if (!assert_true(wf0_fish_world_core_step(right) == 0 &&
                     wf0_fish_world_core_step(left) == 0,
                     "independent cores start at step zero") ||
        !assert_true(wf0_fish_world_tick_once(right, &right_first),
                     "positive x neural decision") ||
        !assert_true(right_first.decision.action.type ==
                         MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
                     right_first.decision.action.move_delta.x == 1000 &&
                     right_first.decision.action.move_delta.y == 0,
                     "positive food delta produces neural +X movement") ||
        !assert_true(right_first.decision.selected_channel ==
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X &&
                     right_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] >
                         right_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X],
                     "positive movement was selected from output spikes") ||
        !assert_true(right_first.core_step_start == 0 &&
                     right_first.core_step_end == 8,
                     "one decision runs exactly eight Core steps") ||
        !assert_true(right_first.action_result.energy_before == UINT64_C(50) &&
                     right_first.action_result.energy_after == UINT64_C(48) &&
                     right_first.energy_after_tick == UINT64_C(47) &&
                     right_first.move_cost == UINT64_C(2) &&
                     right_first.metabolism_cost == UINT64_C(1),
                     "move applies cost followed by metabolism") ||
        !assert_true(wf0_fish_world_tick_once(right, &right_second),
                     "eat decision") ||
        !assert_true(right_second.decision.action.type ==
                         MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
                     right_second.action_result.status ==
                         MINISNN_WORLDS_DOMAIN_ACTION_APPLIED &&
                     right_second.food_nutrition == UINT64_C(15) &&
                     right_second.energy_after_tick == UINT64_C(61) &&
                     right_second.remaining_food == 0U,
                     "neural EAT consumes food and restores energy") ||
        !assert_true(wf0_fish_world_tick_once(left, &left_first),
                     "negative x neural decision") ||
        !assert_true(left_first.decision.action.type ==
                         MINISNN_WORLDS_DOMAIN_ACTION_MOVE &&
                     left_first.decision.action.move_delta.x == -1000 &&
                     left_first.decision.action.move_delta.y == 0,
                     "negative food delta produces neural -X movement") ||
        !assert_true(left_first.decision.selected_channel ==
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X &&
                     left_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_NEG_X] >
                         left_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X],
                     "negative movement was selected from output spikes"))
    {
        goto done;
    }
    passed = 1;

done:
    wf0_fish_world_destroy(&right);
    wf0_fish_world_destroy(&left);
    return passed;
}

static int test_brain_isolation_and_repeatability(void)
{
    WF0FishWorld *first = wf0_fish_world_create(1000, 0);
    WF0FishWorld *second = wf0_fish_world_create(1000, 0);
    WF0FishTickRecord first_record;
    WF0FishTickRecord second_record;
    int passed = 0;

    if (!assert_true(first != NULL && second != NULL, "isolation world creation"))
    {
        goto done;
    }
    if (!assert_true(wf0_fish_world_tick_once(first, &first_record),
                     "first isolated tick") ||
        !assert_true(wf0_fish_world_core_step(first) == 8 &&
                     wf0_fish_world_core_step(second) == 0,
                     "one brain cannot advance another brain") ||
        !assert_true(wf0_fish_world_tick_once(second, &second_record),
                     "second isolated tick") ||
        !assert_true(first_record.decision.action.type ==
                         second_record.decision.action.type &&
                     first_record.decision.selected_channel ==
                         second_record.decision.selected_channel &&
                     first_record.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] ==
                         second_record.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] &&
                     first_record.energy_after_tick ==
                         second_record.energy_after_tick &&
                     first_record.kernel_hash == second_record.kernel_hash &&
                     first_record.domain_hash == second_record.domain_hash,
                     "same seed and inputs produce identical first decisions"))
    {
        goto done;
    }
    passed = 1;

done:
    wf0_fish_world_destroy(&first);
    wf0_fish_world_destroy(&second);
    return passed;
}


static int test_brain_continuity_and_manual_reset(void)
{
    WF0FishWorld *continuous = wf0_fish_world_create(1000, 0);
    WF0FishWorld *reset_world = wf0_fish_world_create(1000, 0);
    WF0FishTickRecord continuous_first;
    WF0FishTickRecord reset_first;
    WF0FishTickRecord continuous_second;
    WF0FishTickRecord reset_second;
    MiniSNNWorldsDomainPerception continuous_perception;
    MiniSNNWorldsDomainPerception reset_perception;
    int passed = 0;

    if (!assert_true(continuous != NULL && reset_world != NULL,
                     "continuity world creation"))
    {
        goto done;
    }
    if (!assert_true(wf0_fish_world_tick_once(continuous, &continuous_first) &&
                     wf0_fish_world_tick_once(reset_world, &reset_first),
                     "matching first decisions") ||
        !assert_true(continuous_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X] ==
                     reset_first.decision.output_scores[
                         MINISNN_WORLDS_BRAIN_ACTION_MOVE_POS_X],
                     "matching first neural state") ||
        !assert_true(wf0_fish_world_perceive(continuous, &continuous_perception) &&
                     wf0_fish_world_perceive(reset_world, &reset_perception) &&
                     continuous_perception.nearest_food_delta_x ==
                         reset_perception.nearest_food_delta_x &&
                     continuous_perception.nearest_food_distance ==
                         reset_perception.nearest_food_distance,
                     "matching second-tick perception") ||
        !assert_true(wf0_fish_world_reset_brain(reset_world),
                     "explicit brain reset") ||
        !assert_true(wf0_fish_world_tick_once(continuous, &continuous_second) &&
                     wf0_fish_world_tick_once(reset_world, &reset_second),
                     "second decisions") ||
        !assert_true(continuous_second.core_step_start == 8 &&
                     continuous_second.core_step_end == 16 &&
                     reset_second.core_step_start == 8 &&
                     reset_second.core_step_end == 16,
                     "manual reset preserves only the Core step counter") ||
        !assert_true(reset_second.decision.cache_hit == 0U,
                     "episode reset clears the cached decision before new neural input") ||
        !assert_true(continuous_second.decision.action.type ==
                         MINISNN_WORLDS_DOMAIN_ACTION_EAT &&
                     reset_second.decision.action.type ==
                         MINISNN_WORLDS_DOMAIN_ACTION_EAT,
                     "manual reset remains a valid explicit scenario operation"))
    {
        goto done;
    }
    passed = 1;

done:
    wf0_fish_world_destroy(&continuous);
    wf0_fish_world_destroy(&reset_world);
    return passed;
}

static int test_dead_actor_stops_brain(void)
{
    WF0FishWorld *world = wf0_fish_world_create(1000, 0);
    WF0FishWorldState state;
    WF0FishTickRecord record;
    int core_before;
    unsigned int step;
    int passed = 0;

    if (!assert_true(world != NULL, "terminal world creation"))
    {
        goto done;
    }
    for (step = 0U; step < 200U; ++step)
    {
        if (!assert_true(wf0_fish_world_tick_once(world, &record), "terminal world tick"))
        {
            goto done;
        }
        if (!assert_true(wf0_fish_world_state(world, &state), "terminal state read"))
        {
            goto done;
        }
        if (state.actor_life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD)
        {
            break;
        }
    }
    if (!assert_true(state.actor_life_state == MINISNN_WORLDS_DOMAIN_LIFE_DEAD,
                     "actor reaches terminal lifecycle") ||
        !assert_true(state.actor_death_cause == MINISNN_WORLDS_DOMAIN_DEATH_CAUSE_STARVATION,
                     "terminal cause is starvation"))
    {
        goto done;
    }
    core_before = wf0_fish_world_core_step(world);
    if (!assert_true(wf0_fish_world_tick_once(world, &record),
                     "dead world advances without a decision") ||
        !assert_true(record.decision_performed == 0 &&
                     record.core_step_start == core_before &&
                     record.core_step_end == core_before &&
                     wf0_fish_world_core_step(world) == core_before,
                     "dead actor does not advance the fixed brain"))
    {
        goto done;
    }
    passed = 1;
done:
    wf0_fish_world_destroy(&world);
    return passed;
}
int main(void)
{
    if (!test_neural_direction_and_metabolism() ||
        !test_brain_isolation_and_repeatability() ||
        !test_brain_continuity_and_manual_reset() ||
        !test_dead_actor_stops_brain())
    {
        return 1;
    }
    printf("WF0 Fish neural causality, continuity, and isolation OK\n");
    return 0;
}

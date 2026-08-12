#include "wd0_test_controller.h"

static MiniSNNWorldsKernelScalar direction(MiniSNNWorldsKernelScalar delta)
{
    return delta > 0 ? 1 : (delta < 0 ? -1 : 0);
}

int wd0_test_controller_choose(
    MiniSNNWorldsKernelEntityId actor,
    const MiniSNNWorldsDomainPerception *perception,
    MiniSNNWorldsKernelScalar eat_range,
    MiniSNNWorldsDomainAction *out_action)
{
    if (perception == NULL || out_action == NULL || eat_range < 0)
    {
        return 0;
    }
    out_action->actor = actor;
    out_action->move_delta.x = 0;
    out_action->move_delta.y = 0;
    out_action->eat_target.value = 0U;
    out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_WAIT;
    if (!perception->nearest_food_present)
    {
        return 1;
    }
    if (perception->nearest_food_distance <= (uint64_t)eat_range)
    {
        out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_EAT;
        out_action->eat_target = perception->nearest_food_entity;
        return 1;
    }
    out_action->type = MINISNN_WORLDS_DOMAIN_ACTION_MOVE;
    out_action->move_delta.x = direction(perception->nearest_food_delta_x);
    out_action->move_delta.y = direction(perception->nearest_food_delta_y);
    return 1;
}
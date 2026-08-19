#include "wf0_fish_internal.h"

int wf0_fish_world_perceive(
    const WF0FishWorld *world,
    MiniSNNWorldsDomainPerception *out_perception)
{
    if (world == NULL || out_perception == NULL)
    {
        return 0;
    }
    return minisnn_worlds_domain_perceive(
               world->domain, world->actor, out_perception) ==
           MINISNN_WORLDS_DOMAIN_ERROR_NONE;
}
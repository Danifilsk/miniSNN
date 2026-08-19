#ifndef WF0_FISH_INTERNAL_H
#define WF0_FISH_INTERNAL_H

#include "wf0_fish.h"
#include "minisnn_worlds_kernel.h"

struct WF0FishWorld
{
    MiniSNNWorldsKernel *kernel;
    MiniSNNWorldsDomain *domain;
    MiniSNN *brain;
    MiniSNNWorldsBrainBridge *brain_bridge;
    int core_step;
    MiniSNNWorldsKernelEntityId actor;
    MiniSNNWorldsKernelEntityId food;
};

#endif
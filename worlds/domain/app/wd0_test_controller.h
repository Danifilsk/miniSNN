#ifndef WD0_TEST_CONTROLLER_H
#define WD0_TEST_CONTROLLER_H

#include "minisnn_worlds_domain.h"

int wd0_test_controller_choose(
    MiniSNNWorldsKernelEntityId actor,
    const MiniSNNWorldsDomainPerception *perception,
    MiniSNNWorldsKernelScalar eat_range,
    MiniSNNWorldsDomainAction *out_action);

#endif
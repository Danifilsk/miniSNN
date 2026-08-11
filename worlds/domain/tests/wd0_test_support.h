#ifndef WD0_TEST_SUPPORT_H
#define WD0_TEST_SUPPORT_H

#include "minisnn_worlds_domain.h"

#include <stdio.h>

#define WD0_CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "WD0 test failed: %s at line %d\n", #condition, __LINE__); \
    return 1; } } while (0)

MiniSNNWorldsKernelEntityId wd0_id(uint64_t value)
{
    MiniSNNWorldsKernelEntityId result = { value };
    return result;
}

MiniSNNWorldsKernelTransform wd0_at(MiniSNNWorldsKernelScalar x,
                                            MiniSNNWorldsKernelScalar y)
{
    MiniSNNWorldsKernelTransform result;
    result.position.x = x;
    result.position.y = y;
    result.orientation = UINT32_C(0);
    return result;
}

MiniSNNWorldsKernel *wd0_kernel(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelError error;

    config.space_bounds.min_x = -1000000;
    config.space_bounds.min_y = -1000000;
    config.space_bounds.max_x = 1000000;
    config.space_bounds.max_y = 1000000;
    return minisnn_worlds_kernel_create(&config, &error);
}

int wd0_make_entity(MiniSNNWorldsKernel *kernel,
                           MiniSNNWorldsKernelScalar x,
                           MiniSNNWorldsKernelScalar y,
                           MiniSNNWorldsKernelEntityId *out_id)
{
    MiniSNNWorldsKernelCommandId command;
    size_t count;

    if (minisnn_worlds_kernel_queue_create_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            wd0_id(0U), &command) != MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    count = minisnn_worlds_kernel_entity_count(kernel);
    if (count == 0U || minisnn_worlds_kernel_entity_at(kernel, count - 1U, out_id) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_queue_place_entity(
            kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
            wd0_id(0U), *out_id, wd0_at(x, y), &command) !=
        MINISNN_WORLDS_KERNEL_ERROR_NONE ||
        minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
    {
        return 0;
    }
    return 1;
}

int wd0_set_blocker(MiniSNNWorldsKernel *kernel,
                           MiniSNNWorldsKernelEntityId entity_id,
                           MiniSNNWorldsKernelScalar half_x,
                           MiniSNNWorldsKernelScalar half_y,
                           uint32_t category_bits, uint32_t blocking_mask)
{
    MiniSNNWorldsKernelCommandId command;
    MiniSNNWorldsKernelOccupancy occupancy;

    occupancy.half_extent_x = half_x;
    occupancy.half_extent_y = half_y;
    occupancy.category_bits = category_bits;
    occupancy.blocking_mask = blocking_mask;
    return minisnn_worlds_kernel_queue_set_occupancy(
               kernel, minisnn_worlds_kernel_tick(kernel) + UINT64_C(1), 0U,
               wd0_id(0U), entity_id, occupancy, &command) ==
               MINISNN_WORLDS_KERNEL_ERROR_NONE &&
           minisnn_worlds_kernel_step(kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE;
}

MiniSNNWorldsDomainSpeciesConfig wd0_species(void)
{
    MiniSNNWorldsDomainSpeciesConfig result;
    result.species_id = UINT64_C(1);
    result.max_energy = UINT64_C(100);
    result.metabolism_per_tick = UINT64_C(1);
    result.move_energy_cost = UINT64_C(3);
    result.eat_range = 2000;
    return result;
}

MiniSNNWorldsDomain *wd0_domain_with_species(MiniSNNWorldsKernel *kernel)
{
    MiniSNNWorldsDomainError error;
    MiniSNNWorldsDomain *domain = minisnn_worlds_domain_create(kernel, &error);
    MiniSNNWorldsDomainSpeciesConfig species = wd0_species();
    if (domain == NULL || error != MINISNN_WORLDS_DOMAIN_ERROR_NONE ||
        minisnn_worlds_domain_add_species(domain, &species) != MINISNN_WORLDS_DOMAIN_ERROR_NONE)
    {
        minisnn_worlds_domain_destroy(domain);
        return NULL;
    }
    return domain;
}

#endif

#include "minisnn_worlds_kernel.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_MINIMUM_READABLE_SIZE \
    (offsetof(MiniSNNWorldsKernelConfig, format_version) + \
     sizeof(((MiniSNNWorldsKernelConfig *)0)->format_version))

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "Worlds Kernel test failed: %s at line %d\n", #condition, __LINE__); \
            return 1; \
        } \
    } while (0)

static int advance(MiniSNNWorldsKernel *kernel, int count)
{
    int index;

    for (index = 0; index < count; ++index)
    {
        if (minisnn_worlds_kernel_step(kernel) != MINISNN_WORLDS_KERNEL_ERROR_NONE)
        {
            return 0;
        }
    }
    return 1;
}

static int diagnostics_match(
    const MiniSNNWorldsKernelDiagnostics *left,
    const MiniSNNWorldsKernelDiagnostics *right)
{
    return left->completed_ticks == right->completed_ticks &&
           left->state == right->state &&
           left->last_error == right->last_error;
}

int main(void)
{
    MiniSNNWorldsKernelConfig config = minisnn_worlds_kernel_config_default();
    MiniSNNWorldsKernelConfig invalid_config;
    MiniSNNWorldsKernelDiagnostics diagnostics;
    MiniSNNWorldsKernelDiagnostics preserved_diagnostics;
    MiniSNNWorldsKernelDiagnostics exact_diagnostics;
    MiniSNNWorldsKernelDiagnostics larger_diagnostics;
    MiniSNNWorldsKernelDiagnostics second_diagnostics;
    MiniSNNWorldsKernelDiagnostics continued_diagnostics;
    MiniSNNWorldsKernelError error = MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    MiniSNNWorldsKernel *default_kernel;
    MiniSNNWorldsKernel *exact_kernel;
    MiniSNNWorldsKernel *larger_kernel;
    MiniSNNWorldsKernel *first;
    MiniSNNWorldsKernel *second;
    MiniSNNWorldsKernel *continued;
    unsigned char *larger_config;
    void *truncated_config;
    size_t larger_config_size;
    uint32_t declared_size;
    uint32_t format_version;

    CHECK(config.struct_size == sizeof(MiniSNNWorldsKernelConfig));
    CHECK(config.format_version == MINISNN_WORLDS_KERNEL_CONFIG_VERSION);
    CHECK(minisnn_worlds_kernel_tick(NULL) == MINISNN_WORLDS_TICK_INITIAL);
    CHECK(minisnn_worlds_kernel_state(NULL) == MINISNN_WORLDS_KERNEL_STATE_FAULTED);
    CHECK(minisnn_worlds_kernel_last_error(NULL) == MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(minisnn_worlds_kernel_step(NULL) == MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);

    default_kernel = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(default_kernel != NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_tick(default_kernel) == MINISNN_WORLDS_TICK_INITIAL);

    exact_kernel = minisnn_worlds_kernel_create(&config, &error);
    CHECK(exact_kernel != NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    invalid_config = config;
    invalid_config.struct_size = UINT32_C(3);
    CHECK(minisnn_worlds_kernel_create(&invalid_config, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);

    invalid_config = config;
    invalid_config.struct_size = (uint32_t)(CONFIG_MINIMUM_READABLE_SIZE - 1U);
    CHECK(minisnn_worlds_kernel_create(&invalid_config, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);

    truncated_config = malloc(sizeof(uint32_t));
    CHECK(truncated_config != NULL);
    declared_size = (uint32_t)sizeof(uint32_t);
    memcpy(truncated_config, &declared_size, sizeof(declared_size));
    CHECK(minisnn_worlds_kernel_create(
              (const MiniSNNWorldsKernelConfig *)truncated_config, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);
    free(truncated_config);

    invalid_config = config;
    invalid_config.format_version += UINT32_C(1);
    CHECK(minisnn_worlds_kernel_create(&invalid_config, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);

    minisnn_worlds_kernel_testing_fail_next_allocation();
    CHECK(minisnn_worlds_kernel_create(&invalid_config, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_INVALID_CONFIG);
    CHECK(minisnn_worlds_kernel_create(NULL, &error) == NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_ALLOCATION);

    larger_config_size = sizeof(MiniSNNWorldsKernelConfig) + 16U;
    larger_config = malloc(larger_config_size);
    CHECK(larger_config != NULL);
    memset(larger_config, 0xa5, larger_config_size);
    declared_size = (uint32_t)larger_config_size;
    format_version = MINISNN_WORLDS_KERNEL_CONFIG_VERSION;
    memcpy(larger_config + offsetof(MiniSNNWorldsKernelConfig, struct_size),
           &declared_size, sizeof(declared_size));
    memcpy(larger_config + offsetof(MiniSNNWorldsKernelConfig, format_version),
           &format_version, sizeof(format_version));
    larger_kernel = minisnn_worlds_kernel_create(
        (const MiniSNNWorldsKernelConfig *)larger_config, &error);
    CHECK(larger_kernel != NULL);
    CHECK(error == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(exact_kernel, &exact_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(larger_kernel, &larger_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics_match(&exact_diagnostics, &larger_diagnostics));
    CHECK(minisnn_worlds_kernel_step(exact_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(larger_kernel) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(exact_kernel, &exact_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(larger_kernel, &larger_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics_match(&exact_diagnostics, &larger_diagnostics));
    free(larger_config);

    first = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(first != NULL);
    second = minisnn_worlds_kernel_create(&config, &error);
    CHECK(second != NULL);
    continued = minisnn_worlds_kernel_create(NULL, &error);
    CHECK(continued != NULL);
    CHECK(minisnn_worlds_kernel_state(first) == MINISNN_WORLDS_KERNEL_STATE_READY);
    CHECK(minisnn_worlds_kernel_tick(first) == MINISNN_WORLDS_TICK_INITIAL);
    CHECK(minisnn_worlds_kernel_step(first) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_tick(first) == UINT64_C(1));
    CHECK(minisnn_worlds_kernel_tick(second) == MINISNN_WORLDS_TICK_INITIAL);
    CHECK(advance(second, 10));
    CHECK(minisnn_worlds_kernel_tick(second) == UINT64_C(10));

    diagnostics.completed_ticks = UINT64_C(99);
    diagnostics.state = MINISNN_WORLDS_KERNEL_STATE_FAULTED;
    diagnostics.last_error = MINISNN_WORLDS_KERNEL_ERROR_INTERNAL;
    preserved_diagnostics = diagnostics;
    CHECK(minisnn_worlds_kernel_get_diagnostics(NULL, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(diagnostics_match(&diagnostics, &preserved_diagnostics));
    CHECK(minisnn_worlds_kernel_get_diagnostics(first, NULL) ==
          MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    CHECK(minisnn_worlds_kernel_get_diagnostics(first, &diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(diagnostics.completed_ticks == UINT64_C(1));
    CHECK(diagnostics.state == MINISNN_WORLDS_KERNEL_STATE_READY);
    CHECK(diagnostics.last_error == MINISNN_WORLDS_KERNEL_ERROR_NONE);

    CHECK(minisnn_worlds_kernel_testing_set_tick(first, UINT64_MAX - UINT64_C(1)) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_step(first) == MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_tick(first) == UINT64_MAX);
    CHECK(minisnn_worlds_kernel_step(first) == MINISNN_WORLDS_KERNEL_ERROR_TICK_OVERFLOW);
    CHECK(minisnn_worlds_kernel_tick(first) == UINT64_MAX);
    CHECK(minisnn_worlds_kernel_state(first) == MINISNN_WORLDS_KERNEL_STATE_READY);
    CHECK(minisnn_worlds_kernel_last_error(first) == MINISNN_WORLDS_KERNEL_ERROR_TICK_OVERFLOW);

    CHECK(advance(second, 990));
    CHECK(advance(continued, 400));
    CHECK(advance(continued, 600));
    CHECK(minisnn_worlds_kernel_get_diagnostics(second, &second_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(minisnn_worlds_kernel_get_diagnostics(continued, &continued_diagnostics) ==
          MINISNN_WORLDS_KERNEL_ERROR_NONE);
    CHECK(second_diagnostics.completed_ticks == UINT64_C(1000));
    CHECK(continued_diagnostics.completed_ticks == UINT64_C(1000));
    CHECK(second_diagnostics.state == continued_diagnostics.state);
    CHECK(second_diagnostics.last_error == continued_diagnostics.last_error);

    minisnn_worlds_kernel_destroy(NULL);
    minisnn_worlds_kernel_destroy(default_kernel);
    minisnn_worlds_kernel_destroy(exact_kernel);
    minisnn_worlds_kernel_destroy(larger_kernel);
    minisnn_worlds_kernel_destroy(first);
    minisnn_worlds_kernel_destroy(second);
    minisnn_worlds_kernel_destroy(continued);
    printf("Worlds Kernel configuration prefix and tail validation OK\n");
    printf("Worlds Kernel lifecycle and logical tick validation OK\n");
    return 0;
}

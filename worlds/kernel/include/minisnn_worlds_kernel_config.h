#ifndef MINISNN_WORLDS_KERNEL_CONFIG_H
#define MINISNN_WORLDS_KERNEL_CONFIG_H

#include <stdint.h>

#define MINISNN_WORLDS_KERNEL_CONFIG_VERSION UINT32_C(1)

typedef struct
{
    uint32_t struct_size;
    uint32_t format_version;
} MiniSNNWorldsKernelConfig;

MiniSNNWorldsKernelConfig minisnn_worlds_kernel_config_default(void);

#endif

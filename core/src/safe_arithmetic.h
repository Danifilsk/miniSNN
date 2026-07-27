#ifndef MINISNN_SAFE_ARITHMETIC_H
#define MINISNN_SAFE_ARITHMETIC_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static inline int minisnn_checked_add_size(size_t left, size_t right, size_t *out_value)
{
    if (out_value == NULL || left > SIZE_MAX - right)
        return 0;
    *out_value = left + right;
    return 1;
}

static inline int minisnn_checked_mul_size(size_t left, size_t right, size_t *out_value)
{
    if (out_value == NULL || (left != 0U && right > SIZE_MAX / left))
        return 0;
    *out_value = left * right;
    return 1;
}

static inline int minisnn_checked_increment_u64(uint64_t value, uint64_t *out_value)
{
    if (out_value == NULL || value == UINT64_MAX)
        return 0;
    *out_value = value + UINT64_C(1);
    return 1;
}

static inline int minisnn_checked_increment_int(int value, int *out_value)
{
    if (out_value == NULL || value == INT_MAX)
        return 0;
    *out_value = value + 1;
    return 1;
}

#endif

#define MINISNN_TEST_ALLOCATION_IMPLEMENTATION
#include "test_allocation.h"

#ifdef MINISNN_TESTING
#include <stdint.h>

/* This translation unit is also compiled with the global test include. */
#ifdef malloc
#undef malloc
#endif
#ifdef calloc
#undef calloc
#endif
#ifdef realloc
#undef realloc
#endif

static size_t allocation_count;
static size_t fail_after = SIZE_MAX;

static int allocation_allowed(void)
{
    if (allocation_count >= fail_after)
        return 0;
    allocation_count++;
    return 1;
}

void minisnn_test_allocation_fail_after(size_t successful_allocations)
{
    allocation_count = 0U;
    fail_after = successful_allocations;
}

void minisnn_test_allocation_reset(void)
{
    allocation_count = 0U;
    fail_after = SIZE_MAX;
}

size_t minisnn_test_allocation_count(void)
{
    return allocation_count;
}

void *minisnn_test_malloc(size_t size)
{
    return allocation_allowed() ? malloc(size) : NULL;
}

void *minisnn_test_calloc(size_t count, size_t size)
{
    return allocation_allowed() ? calloc(count, size) : NULL;
}

void *minisnn_test_realloc(void *pointer, size_t size)
{
    return allocation_allowed() ? realloc(pointer, size) : NULL;
}
#endif

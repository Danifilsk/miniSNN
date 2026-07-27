#ifndef MINISNN_TEST_ALLOCATION_H
#define MINISNN_TEST_ALLOCATION_H

#include <stddef.h>
#include <stdlib.h>

#ifdef MINISNN_TESTING
void minisnn_test_allocation_fail_after(size_t successful_allocations);
void minisnn_test_allocation_reset(void);
size_t minisnn_test_allocation_count(void);
void *minisnn_test_malloc(size_t size);
void *minisnn_test_calloc(size_t count, size_t size);
void *minisnn_test_realloc(void *pointer, size_t size);

#ifndef MINISNN_TEST_ALLOCATION_IMPLEMENTATION
#define malloc(size) minisnn_test_malloc(size)
#define calloc(count, size) minisnn_test_calloc((count), (size))
#define realloc(pointer, size) minisnn_test_realloc((pointer), (size))
#endif
#endif

#endif

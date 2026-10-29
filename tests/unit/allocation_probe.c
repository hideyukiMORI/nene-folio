#include "allocation_probe.h"

#include <stdint.h>
#include <stdlib.h>

static size_t countdown = 0; /* 0 なら失敗させない */
static size_t calls = 0;
static size_t malloc_size = 0;
static size_t realloc_size = 0;
static size_t calloc_max_bytes = 0;

void allocation_probe_fail_at(size_t nth)
{
    countdown = nth;
    calls = 0;
    malloc_size = 0;
    realloc_size = 0;
    calloc_max_bytes = 0;
}

size_t allocation_probe_calls(void)
{
    return calls;
}

size_t allocation_probe_malloc_size(void)
{
    return malloc_size;
}

size_t allocation_probe_realloc_size(void)
{
    return realloc_size;
}

size_t allocation_probe_calloc_max_bytes(void)
{
    return calloc_max_bytes;
}

static bool should_fail(void)
{
    if (countdown == 0)
    {
        return false;
    }
    countdown -= 1;
    return countdown == 0;
}

void *_Nullable folio_probe_malloc(size_t size)
{
    calls += 1;
    malloc_size = size;
    return should_fail() ? nullptr : malloc(size);
}

void *_Nullable folio_probe_calloc(size_t count, size_t size)
{
    calls += 1;
    size_t requested = size != 0 && count > SIZE_MAX / size ? SIZE_MAX : count * size;
    if (requested > calloc_max_bytes)
        calloc_max_bytes = requested;
    return should_fail() ? nullptr : calloc(count, size);
}

void *_Nullable folio_probe_realloc(void *_Nullable block, size_t size)
{
    calls += 1;
    realloc_size = size;
    return should_fail() ? nullptr : realloc(block, size);
}

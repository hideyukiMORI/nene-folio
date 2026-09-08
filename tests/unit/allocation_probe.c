#include "allocation_probe.h"

#include <stdlib.h>

static size_t countdown = 0; /* 0 なら失敗させない */

void allocation_probe_fail_at(size_t nth)
{
    countdown = nth;
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
    return should_fail() ? nullptr : malloc(size);
}

void *_Nullable folio_probe_calloc(size_t count, size_t size)
{
    return should_fail() ? nullptr : calloc(count, size);
}

void *_Nullable folio_probe_realloc(void *_Nullable block, size_t size)
{
    return should_fail() ? nullptr : realloc(block, size);
}

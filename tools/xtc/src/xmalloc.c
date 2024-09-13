#include "xmalloc.h"

#include <stdlib.h>

void *xmalloc(size_t sz)
{
    void *p = malloc(sz);
    if (!p) abort();
    return p;
}

void *xrealloc(void *p, size_t sz)
{
    p = realloc(p, sz);
    if (!p) abort();
    return p;
}

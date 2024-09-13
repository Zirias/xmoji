#ifndef XTC_XMALLOC_H
#define XTC_XMALLOC_H

#include <stddef.h>

void *xmalloc(size_t sz);
void *xrealloc(void *p, size_t sz);

#endif

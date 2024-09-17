#ifndef EMOJIGEN_UTIL_H
#define EMOJIGEN_UTIL_H

#include <stddef.h>

void *xmalloc(size_t sz);
void *xrealloc(void *p, size_t sz);
void usage(const char *name);

#endif

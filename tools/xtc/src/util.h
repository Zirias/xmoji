#ifndef XTC_UTIL_H
#define XTC_UTIL_H

#include <stddef.h>

void *xmalloc(size_t sz);
void *xrealloc(void *p, size_t sz);
void usage(const char *name);
char *derivename(const char *filename, const char *lang,
	const char *dir, const char *ext);

#endif

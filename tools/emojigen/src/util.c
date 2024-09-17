#include "util.h"

#include <stdio.h>
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

void usage(const char *name)
{
    fprintf(stderr, "usage: %s source outname emoji-test.txt\n"
		    "       %s groupnames strings.def strings.def.in emoji-test.txt\n"
		    "       %s translate strings-lang.def emoji-test.txt lang.xml [lang.xml ...]\n",
		    name, name, name);
    exit(EXIT_FAILURE);
}


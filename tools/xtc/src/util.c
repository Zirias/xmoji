#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    fprintf(stderr, "usage: %s source outname namespace strings.def\n"
		    "       %s update lang strings.def\n"
		    "       %s compile outdir lang strings.def\n",
		    name, name, name);
    exit(EXIT_FAILURE);
}

char *derivename(const char *filename, const char *lang,
	const char *dir, const char *ext)
{
    char *slash = strrchr(filename, '/');
    size_t dirlen = 0;
    if (dir) dirlen = strlen(dir);
    if (slash)
    {
	if (!dir)
	{
	    dirlen = (size_t)(slash - filename);
	    dir = filename;
	}
	filename = slash+1;
    }
    char *dot = strrchr(filename, '.');
    size_t extlen = 0;
    size_t nmlen = 0;
    if (ext) extlen = strlen(ext);
    if (dot)
    {
	if (!ext)
	{
	    extlen = strlen(dot);
	    ext = dot;
	}
	nmlen = (size_t)(dot - filename);
    } else nmlen = strlen(filename);
    size_t langlen = strlen(lang);
    size_t reslen = dirlen + !!dirlen + nmlen + 1 + langlen + extlen;
    char *res = xmalloc(reslen+1);
    size_t respos = 0;
    if (dirlen)
    {
	memcpy(res, dir, dirlen);
	res[dirlen] = '/';
	respos += dirlen + 1;
    }
    memcpy(res+respos, filename, nmlen);
    res[respos+nmlen] = '-';
    respos += nmlen + 1;
    memcpy(res+respos, lang, langlen);
    respos += langlen;
    if (extlen)
    {
	memcpy(res+respos, ext, extlen);
	respos += extlen;
    }
    res[respos] = 0;
    return res;
}

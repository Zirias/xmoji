#include "deffile.h"

#include "xmalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNKSZ 128

struct DefEntry
{
    DefEntry *next;
    char *key;
    char *from;
    char *to;
    DefType type;
    unsigned id;
};

struct DefFile
{
    size_t len;
    DefEntry *entries;
    DefEntry *bucket[256];
};

static unsigned char hash(const char *key)
{
    size_t h = 5381;
    while (*key) h += (h << 5) + ((unsigned char)*key++);
    return h;
}

DefFile *DefFile_create(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
	fprintf(stderr, "Error opening `%s' for reading\n", filename);
	return 0;
    }

    enum { PS_START, PS_KEY, PS_FROM, PS_TO } parsestate = PS_START;
    static char buf[1024];
    char *key = 0;
    char *from = 0;
    char *to = 0;
    size_t keylen = 0;
    size_t fromlen = 0;
    size_t tolen = 0;
    size_t linelen = 0;
    size_t newlen = 0;
    DefType type = DT_CHAR;
    int haveeol = 0;
    int hadeol = 0;
    int firstline = 0;

    DefFile *self = xmalloc(sizeof *self);
    memset(self, 0, sizeof *self);
    size_t capa = 0;
    
    for (unsigned lineno = 0; fgets(buf, sizeof buf, f); lineno += haveeol)
    {
	hadeol = haveeol;
	char *nl = strchr(buf, '\n');
	haveeol = !!nl;
	*nl = 0;

	switch (parsestate)
	{
	    case PS_START:
		if (buf[0] != '$') continue;
		if (buf[2] != '$')
		{
		    fprintf(stderr, "%s:%u: malformed line\n",
			    filename, lineno);
		    goto error;
		}
		switch (buf[1])
		{
		    case 'c':
			type = DT_CHAR; break;
		    case 'w':
			type = DT_CHAR32; break;
		    default:
			fprintf(stderr, "%s:%u: malformed line\n",
				filename, lineno);
			goto error;
		}
		keylen = strlen(buf+3);
		if (keylen)
		{
		    key = xmalloc(keylen+1);
		    memcpy(key, buf+3, keylen+1);
		}
		else
		{
		    fprintf(stderr, "%s:%u: missing key\n", filename, lineno);
		    goto error;
		}
		parsestate = haveeol ? PS_FROM : PS_KEY;
		firstline = 1;
		break;

	    case PS_KEY:
		linelen = strlen(buf);
		newlen = keylen + linelen;
		if (newlen != keylen)
		{
		    key = xrealloc(key, newlen+1);
		    memcpy(key+keylen, buf, linelen+1);
		    keylen = newlen;
		}
		if (haveeol) parsestate = PS_FROM;
		break;

	    case PS_FROM:
		if (hadeol && buf[0] == '.' && !buf[1])
		{
		    parsestate = PS_TO;
		    firstline = 1;
		    continue;
		}
		if (firstline) hadeol = 0;
		firstline = 0;
		linelen = strlen(buf);
		newlen = fromlen + linelen + hadeol;
		if (newlen == fromlen) continue;
		from = xrealloc(from, newlen+1);
		if (hadeol) from[fromlen++] = '\n';
		memcpy(from+fromlen, buf, linelen+1);
		fromlen = newlen;
		break;

	    case PS_TO:
		if (hadeol && buf[0] == '.' && !buf[1])
		{
		    if (self->len == capa)
		    {
			capa += CHUNKSZ;
			self->entries = xrealloc(self->entries,
				capa * sizeof *self->entries);
		    }
		    DefEntry *entry = self->entries + self->len;
		    unsigned char bucket = hash(key);
		    entry->next = self->bucket[bucket];
		    self->bucket[bucket] = entry;
		    entry->key = key;
		    entry->from = from;
		    entry->to = to;
		    entry->type = type;
		    entry->id = self->len++;
		    key = 0;
		    from = 0;
		    to = 0;
		    fromlen = 0;
		    tolen = 0;
		    parsestate = PS_START;
		    continue;
		}
		if (firstline) hadeol = 0;
		firstline = 0;
		linelen = strlen(buf);
		newlen = tolen + linelen + hadeol;
		if (newlen == tolen) continue;
		to = xrealloc(to, newlen+1);
		if (hadeol) to[tolen++] = '\n';
		memcpy(to+tolen, buf, linelen+1);
		tolen = newlen;
		break;
	}
    }
    if (parsestate != PS_START)
    {
	fprintf(stderr, "%s: unexpected end of file", filename);
	goto error;
    }
    fclose(f);
    self->entries = xrealloc(self->entries, self->len * sizeof *self->entries);
    return self;

error:
    fclose(f);
    free(to);
    free(from);
    DefFile_destroy(self);
    return 0;
}

size_t DefFile_len(const DefFile *self)
{
    return self->len;
}

const DefEntry *DefFile_byId(const DefFile *self, unsigned id)
{
    if (id > self->len) return 0;
    return self->entries + id;
}

const DefEntry *DefFile_byKey(const DefFile *self, const char *key)
{
    DefEntry *entry = self->bucket[hash(key)];
    while (entry)
    {
	if (!strcmp(entry->key, key)) break;
	entry = entry->next;
    }
    return entry;
}

const char *DefEntry_key(const DefEntry *self)
{
    return self->key;
}

const char *DefEntry_from(const DefEntry *self)
{
    return self->from;
}

const char *DefEntry_to(const DefEntry *self)
{
    return self->to;
}

DefType DefEntry_type(const DefEntry *self)
{
    return self->type;
}

unsigned DefEntry_id(const DefEntry *self)
{
    return self->id;
}

void DefFile_destroy(DefFile *self)
{
    if (!self) return;
    free(self->entries);
    free(self);
}


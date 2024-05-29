#include "unistrbuilder.h"

#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

#define BUILDER_CHUNK 128

struct UniStrBuilder
{
    UniStr string;
    size_t capa;
};

UniStrBuilder *UniStrBuilder_create(void)
{
    UniStrBuilder *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->string.refcnt = -1;
    return self;
}

static void adjust(UniStrBuilder *self, size_t newlen)
{
    if (newlen < self->capa && 2 * newlen > self->capa) return;
    size_t newcapa = ((newlen + BUILDER_CHUNK)
	    / BUILDER_CHUNK) * BUILDER_CHUNK;
    if (newcapa < self->capa && 2 * newcapa > self->capa) return;
    self->string.str = PSC_realloc(self->string.str,
	    newcapa * sizeof *self->string.str);
    self->capa = newcapa;
}

void UniStrBuilder_appendChar(UniStrBuilder *self, char32_t c)
{
    adjust(self, self->string.len + 1);
    self->string.str[self->string.len] = c;
    self->string.str[++self->string.len] = 0;
}

void UniStrBuilder_appendStr(UniStrBuilder *self, const char32_t *s)
{
    size_t appendlen = UniStr_utf32len(s);
    adjust(self, self->string.len + appendlen);
    memcpy(self->string.str + self->string.len, s,
	    (appendlen + 1) * sizeof *s);
    self->string.len += appendlen;
}

void UniStrBuilder_insertChar(UniStrBuilder *self,
	size_t pos, char32_t c)
{
    if (pos >= self->string.len)
    {
	UniStrBuilder_appendChar(self, c);
	return;
    }
    adjust(self, self->string.len + 1);
    memmove(self->string.str + pos + 1, self->string.str + pos,
	    (++self->string.len - pos) * sizeof *self->string.str);
    self->string.str[pos] = c;
}

void UniStrBuilder_insertStr(UniStrBuilder *self,
	size_t pos, const char32_t *s)
{
    if (pos >= self->string.len)
    {
	UniStrBuilder_appendStr(self, s);
	return;
    }
    size_t insertlen = UniStr_utf32len(s);
    adjust(self, self->string.len + insertlen);
    memmove(self->string.str + pos + insertlen, self->string.str + pos,
	    (self->string.len + 1 - pos) * sizeof *self->string.str);
    memcpy(self->string.str + pos, s, insertlen * sizeof *s);
    self->string.len += insertlen;
}

void UniStrBuilder_clear(UniStrBuilder *self)
{
    free(self->string.str);
    self->string.len = 0;
    self->string.str = 0;
    self->capa = 0;
}

void UniStrBuilder_remove(UniStrBuilder *self,
	size_t pos, size_t len)
{
    if (!pos && len >= self->string.len)
    {
	UniStrBuilder_clear(self);
	return;
    }
    if (self->string.len - pos < len) len = self->string.len - pos;
    memmove(self->string.str + pos, self->string.str + pos + len,
	    (self->string.len + 1 - pos - len) * sizeof *self->string.str);
    self->string.len -= len;
    adjust(self, self->string.len);
}

UniStr *UniStrBuilder_string(const UniStrBuilder *self)
{
    UniStr *string = PSC_malloc(sizeof *string);
    string->len = self->string.len;
    string->str = PSC_malloc((string->len + 1) * sizeof *string->str);
    string->refcnt = 1;
    memcpy(string->str, self->string.str,
	    (string->len + 1) * sizeof *string->str);
    return string;
}

const UniStr *UniStrBuilder_stringView(const UniStrBuilder *self)
{
    return &self->string;
}

void UniStrBuilder_destroy(UniStrBuilder *self)
{
    if (!self) return;
    free(self->string.str);
    free(self);
}


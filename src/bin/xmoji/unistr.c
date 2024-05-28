#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct UniStr
{
    char32_t *utf32;
    char *utf8;
    size_t utf32len;
    size_t utf8len;
};

static size_t utf32len(const char32_t *utf32)
{
    const char32_t *endp;
    for (endp = utf32; *endp; ++endp);
    return endp - utf32;
}

static size_t toutf8(char **utf8, size_t pos,
	const char32_t *utf32, size_t len)
{
    size_t utf8maxlen = 4*len;
    size_t utf8len = 0;

    *utf8 = PSC_realloc(*utf8, pos + utf8maxlen + 1);
    for (size_t i = 0; i < len; ++i)
    {
	char32_t c = utf32[i];
	if (c < 0x80U)
	{
	    (*utf8)[pos + utf8len++] = c;
	    continue;
	}
	if (c > 0x10ffffU)
	{
	    (*utf8)[pos + utf8len++] = '?';
	    continue;
	}
	unsigned char b[] = {
	    c & 0xffU,
	    c >> 8 & 0xffU,
	    c >> 16 & 0xffU
	};
	if (c < 0x800U)
	{
	    (*utf8)[pos + utf8len++] = 0xc0U | (b[1] << 2) | (b[0] >> 6);
	    goto follow2;
	}
	if (c < 0x8000U)
	{
	    (*utf8)[pos + utf8len++] = 0xe0U | (b[1] >> 4);
	    goto follow1;
	}
	(*utf8)[pos + utf8len++] = 0xf0U | (b[2] >> 2);
	(*utf8)[pos + utf8len++] = 0x80U | ((b[2] << 4) & 0x3fU) | (b[1] >> 4);
follow1:
	(*utf8)[pos + utf8len++] = 0x80U | ((b[1] << 2) & 0x3fU) | (b[0] >> 6);
follow2:
	(*utf8)[pos + utf8len++] = 0x80U | (b[0] & 0x3fU);
    }
    *utf8 = PSC_realloc(*utf8, pos + utf8len + 1);
    (*utf8)[pos + utf8len] = 0;
    return pos + utf8len;
}

static void storechar(void *out, int csz, size_t p, char32_t c)
{
    if (csz == 1) ((char *)out)[p] = c > 0xffU ? '?' : c;
    else ((char32_t *)out)[p] = c;
}

static size_t decodeutf8(void **out, int csz, size_t pos,
	const char *utf8, size_t len)
{
    if (!len && *utf8) len = strlen(utf8);
    size_t outlen = 0;
    const unsigned char *b = (const unsigned char *)utf8;

    *out = PSC_realloc(*out, (pos + len + 1) * csz);
    for (size_t i = 0; i < len; ++i)
    {
	if (b[i] < 0x80U)
	{
	    storechar(*out, csz, pos + outlen++, b[i]);
	    continue;
	}
	char32_t c = 0;
	int f = 0;
	if ((b[i] & 0xe0U) == 0xc0U)
	{
	    c = b[i] & 0x1fU;
	    f = 1;
	}
	else if ((b[i] & 0xf0U) == 0xe0U)
	{
	    c = b[i] & 0xfU;
	    f = 2;
	}
	else if ((b[i] & 0xf8U) == 0xf0U)
	{
	    c = b[i] & 0x7U;
	    f = 3;
	}
	else
	{
	    storechar(*out, csz, pos + outlen++, '?');
	    continue;
	}
	for (; f && ++i < len; --f)
	{
	    if ((b[i] & 0xc0U) != 0x80U) break;
	    c <<= 6;
	    c |= (b[i] & 0x3fU);
	}
	storechar(*out, csz, pos + outlen++, f ? '?' : c);
    }
    *out = PSC_realloc(*out, (pos + outlen + 1) * csz);
    storechar(*out, csz, pos + outlen, 0);
    return pos + outlen;
}

static size_t toutf32(char32_t **utf32, size_t pos,
	const char *utf8, size_t len)
{
    void *out = *utf32;
    size_t outlen = decodeutf8(&out, sizeof **utf32, pos, utf8, len);
    *utf32 = out;
    return outlen;
}

UniStr *UniStr_createEmpty(void)
{
    UniStr *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    return self;
}

UniStr *UniStr_fromUtf8(const char *utf8)
{
    UniStr *self = UniStr_createEmpty();
    self->utf8len = strlen(utf8);
    self->utf8 = PSC_malloc(self->utf8len + 1);
    memcpy(self->utf8, utf8, self->utf8len);
    self->utf8[self->utf8len] = 0;
    return self;
}

UniStr *UniStr_fromUtf32(const char32_t *utf32)
{
    UniStr *self = UniStr_createEmpty();
    self->utf32len = utf32len(utf32);
    self->utf32 = PSC_malloc((self->utf32len + 1) * sizeof *self->utf32);
    memcpy(self->utf32, utf32, self->utf32len * sizeof *self->utf32);
    self->utf32[self->utf32len] = 0;
    return self;
}

void UniStr_appendUtf8(UniStr *self, const char *utf8)
{
    size_t len = strlen(utf8);
    self->utf8 = PSC_realloc(self->utf8, self->utf8len + len + 1);
    memcpy(self->utf8 + self->utf8len, utf8, len);
    if (self->utf32len)
    {
	self->utf32len = toutf32(&self->utf32, self->utf32len, utf8, len);
    }
    self->utf8len += len;
    self->utf8[self->utf8len] = 0;
}

void UniStr_appendUtf32(UniStr *self, const char32_t *utf32)
{
    size_t len = utf32len(utf32);
    self->utf32 = PSC_realloc(self->utf32,
	    (self->utf32len + len + 1) * sizeof *self->utf32);
    memcpy(self->utf32 + self->utf32len, utf32, len * sizeof *self->utf32);
    if (self->utf8len)
    {
	self->utf8len = toutf8(&self->utf8, self->utf8len, utf32, len);
    }
    self->utf32len += len;
    self->utf32[self->utf32len] = 0;
}

static void initutf32(UniStr *self)
{
    if (self->utf32len || !self->utf8len) return;
    self->utf32len = toutf32(&self->utf32, 0, self->utf8, self->utf8len);
}

static void initutf8(UniStr *self)
{
    if (self->utf8len || !self->utf32len) return;
    self->utf8len = toutf8(&self->utf8, 0, self->utf32, self->utf32len);
}

void UniStr_convert(UniStr *self)
{
    initutf32(self);
    initutf8(self);
}

size_t UniStr_mutf8len(UniStr *self)
{
    initutf8(self);
    return self->utf8len;
}

const char *UniStr_mutf8(UniStr *self)
{
    initutf8(self);
    return self->utf8;
}

size_t UniStr_mutf32len(UniStr *self)
{
    initutf32(self);
    return self->utf32len;
}

const char32_t *UniStr_mutf32(UniStr *self)
{
    initutf32(self);
    return self->utf32;
}

size_t UniStr_cutf8len(const UniStr *self)
{
    return self->utf8len;
}

const char *UniStr_cutf8(const UniStr *self)
{
    return self->utf8;
}

size_t UniStr_cutf32len(const UniStr *self)
{
    return self->utf32len;
}

const char32_t *UniStr_cutf32(const UniStr *self)
{
    return self->utf32;
}

void UniStr_destroy(UniStr *self)
{
    if (!self) return;
    free(self->utf32);
    free(self->utf8);
    free(self);
}

char *UniStr_toLatin1(const UniStr *self)
{
    char *latin1 = 0;
    if (self->utf32)
    {
	latin1 = PSC_malloc(self->utf32len+1);
	for (size_t i = 0; i < self->utf32len; ++i)
	{
	    if (self->utf32[i] > 0xffU) latin1[i] = '?';
	    else latin1[i] = self->utf32[i];
	}
	latin1[self->utf32len] = 0;
    }
    else latin1 = UniStr_utf8ToLatin1(self->utf8);
    return latin1;
}

char *UniStr_utf8ToLatin1(const char *utf8)
{
    void *out = 0;
    if (utf8) decodeutf8(&out, 1, 0, utf8, 0);
    return out;
}


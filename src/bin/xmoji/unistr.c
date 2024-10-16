#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

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
	if (c > 0x10ffffU) c = 0xfffdU;
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
	if (c < 0x10000U)
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
	    storechar(*out, csz, pos + outlen++, 0xfffdU);
	    continue;
	}
	for (; f && ++i < len; --f)
	{
	    if ((b[i] & 0xc0U) != 0x80U) break;
	    c <<= 6;
	    c |= (b[i] & 0x3fU);
	}
	storechar(*out, csz, pos + outlen++, f ? 0xfffdU : c);
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

static UniStr *create(char32_t *str, size_t len)
{
    UniStr *self = PSC_malloc(sizeof *self);
    self->len = len;
    self->str = str;
    self->refcnt = 1;
    return self;
}

UniStr *UniStr_createFromUtf8(const char *utf8)
{
    if (!*utf8) return create(0, 0);
    char32_t *str = 0;
    size_t len = toutf32(&str, 0, utf8, strlen(utf8));
    return create(str, len);
}

UniStr *UniStr_createFromUtf32(const char32_t *utf32)
{
    if (!*utf32) return create(0, 0);
    size_t len = UniStr_utf32len(utf32);
    char32_t *str = PSC_malloc((len + 1) * sizeof *str);
    memcpy(str, utf32, len * sizeof *str);
    return create(str, len);
}

UniStr *UniStr_createOwned(char32_t *utf32)
{
    return create(utf32, UniStr_utf32len(utf32));
}

UniStr *UniStr_createFromLatin1(const char *latin1, size_t len)
{
    if (!len) return create(0, 0);
    char32_t *str = PSC_malloc((len + 1) *sizeof *str);
    for (size_t i = 0; i < len; ++i) str[i] = latin1[i];
    str[len] = 0;
    return create(str, len);
}

UniStr *UniStr_ref(const UniStr *self)
{
    UniStr *ref = (UniStr *)self;
    if (ref->refcnt > 0) ++ref->refcnt;
    return ref;
}

size_t UniStr_len(const UniStr *self)
{
    return self->len;
}

const char32_t *UniStr_str(const UniStr *self)
{
    return self->len ? self->str : U"";
}

static UniStr *clone(const UniStr *self, int always)
{
    if (self->refcnt < 0 && !always) return (UniStr *)self;
    UniStr *ustr = PSC_malloc(sizeof *ustr);
    ustr->len = self->len;
    ustr->str = PSC_malloc((self->len + 1) * sizeof *ustr->str);
    memcpy(ustr->str, self->str, (self->len + 1) * sizeof *ustr->str);
    ustr->refcnt = 1;
    return ustr;
}

UniStr *UniStr_appendUtf8(const UniStr *self, const char *utf8)
{
    UniStr *ustr = clone(self, 1);
    ustr->len = toutf32(&ustr->str, ustr->len, utf8, strlen(utf8));
    return ustr;
}

UniStr *UniStr_appendUtf32(const UniStr *self, const char32_t *utf32)
{
    size_t addlen = UniStr_utf32len(utf32);
    UniStr *ustr = PSC_malloc(sizeof *ustr);
    ustr->len = self->len + addlen;
    ustr->str = PSC_malloc((self->len + addlen + 1) * sizeof *ustr->str);
    memcpy(ustr->str, self->str, self->len * sizeof *ustr->str);
    memcpy(ustr->str + self->len, utf32, (addlen + 1) * sizeof *ustr->str);
    ustr->refcnt = 1;
    return ustr;
}

static void destroy(void *obj)
{
    UniStr_destroy(obj);
}

PSC_List *UniStr_splitByUtf8(const UniStr *self, const char *delim)
{
    if (!self->len) return PSC_List_create();
    size_t delimlen = strlen(delim);
    if (!delimlen || delimlen > self->len)
    {
	PSC_List *split = PSC_List_create();
	PSC_List_append(split, clone(self, 0), destroy);
	return split;
    }
    char32_t *utf32delim = 0;
    toutf32(&utf32delim, 0, delim, delimlen);
    PSC_List *split = UniStr_splitByUtf32(self, utf32delim);
    free(utf32delim);
    return split;
}

PSC_List *UniStr_splitByUtf32(const UniStr *self, const char32_t *delim)
{
    PSC_List *split = PSC_List_create();
    if (!self->len) goto done;
    if (!*delim)
    {
	PSC_List_append(split, clone(self, 0), destroy);
	goto done;
    }
    size_t delimlen = UniStr_utf32len(delim);
    size_t pos = 0;
    size_t start = 0;
    while (pos + delimlen <= self->len)
    {
	if (!memcmp(self->str + pos, delim, delimlen * sizeof *self->str))
	{
	    if (pos == start) PSC_List_append(split, create(0, 0), destroy);
	    else
	    {
		char32_t *splitstr = PSC_malloc(
			(pos - start + 1) * sizeof *splitstr);
		memcpy(splitstr, self->str + start,
			(pos - start) * sizeof *splitstr);
		splitstr[pos - start] = 0;
		PSC_List_append(split, create(splitstr, pos - start), destroy);
	    }
	    pos += delimlen;
	    start = pos;
	}
	else ++pos;
    }
    if (start == 0) PSC_List_append(split, clone(self, 0), destroy);
    else if (start < self->len)
    {
	char32_t *splitstr = PSC_malloc(
		(self->len - start + 1) * sizeof *splitstr);
	memcpy(splitstr, self->str + start,
		(self->len - start + 1) * sizeof *splitstr);
	PSC_List_append(split, create(splitstr, self->len - start), destroy);
    }
    else PSC_List_append(split, create(0, 0), destroy);

done:
    return split;
}

UniStr *UniStr_cutByUtf8(const UniStr *self, const char *delims)
{
    if (!self->len) return UniStr_ref(self);
    size_t delimslen = strlen(delims);
    char32_t *utf32delims = 0;
    toutf32(&utf32delims, 0, delims, delimslen);
    UniStr *cut = UniStr_cutByUtf32(self, utf32delims);
    free(utf32delims);
    return cut;
}

UniStr *UniStr_cutByUtf32(const UniStr *self, const char32_t *delims)
{
    if (!self->len) return UniStr_ref(self);
    size_t cutlen = 0;
    while (self->str[cutlen])
    {
	const char32_t *d = delims;
	while (*d) if (self->str[cutlen] == *d++) goto done;
	++cutlen;
    }
done:
    if (cutlen < self->len)
    {
	char32_t *cutstr = PSC_malloc((cutlen + 1) * sizeof *cutstr);
	memcpy(cutstr, self->str, cutlen * sizeof *cutstr);
	cutstr[cutlen] = 0;
	return create(cutstr, cutlen);
    }
    return UniStr_ref(self);
}

void UniStr_destroy(UniStr *self)
{
    if (!self || self->refcnt < 0 || --self->refcnt) return;
    free(self->str);
    free(self);
}

char *UniStr_toLatin1(const UniStr *self)
{
    if (!self->len) return 0;
    char *latin1 = PSC_malloc(self->len+1);
    for (size_t i = 0; i < self->len; ++i)
    {
	if (self->str[i] > 0xffU) latin1[i] = '?';
	else latin1[i] = self->str[i];
    }
    latin1[self->len] = 0;
    return latin1;
}

char *UniStr_utf8ToLatin1(const char *utf8)
{
    void *out = 0;
    if (utf8 && *utf8) decodeutf8(&out, 1, 0, utf8, 0);
    return out;
}

char *UniStr_toUtf8(const UniStr *self, size_t *len)
{
    if (!self->len)
    {
	if (len) *len = 0;
	return 0;
    }
    char *utf8 = 0;
    size_t utf8len = toutf8(&utf8, 0, self->str, self->len);
    if (len) *len = utf8len;
    return utf8;
}

size_t UniStr_utf32len(const char32_t *s)
{
    const char32_t *endp;
    for (endp = s; *endp; ++endp);
    return endp - s;
}

int UniStr_equals(const UniStr *str, const UniStr *other)
{
    if (str == other) return 1;
    if (!str || !other) return 0;
    if (str->len != other->len) return 0;
    return !memcmp(str->str, other->str, str->len * sizeof *str->str);
}

static char32_t isolc(char32_t c)
{
    if ((c >= 0x41 && c <= 0x5a)
	    || (c >= 0xc0 && c <= 0xd6)
	    || (c >= 0xd8 && c <= 0xde)) return c + 0x20;
    return c;
}

int UniStr_containslc(const UniStr *big, const UniStr *little)
{
    if (big == little) return 1;
    if (!big || !big->len || !little || !little->len || little->len > big->len)
    {
	return 0;
    }
    size_t steps = big->len - little->len + 1;
    for (size_t start = 0; start < steps; ++start)
    {
	int equals = 1;
	for (size_t j = 0; j < little->len; ++j)
	{
	    char32_t a = isolc(big->str[start+j]);
	    char32_t b = isolc(little->str[j]);
	    if (a != b)
	    {
		equals = 0;
		break;
	    }
	}
	if (equals) return 1;
    }
    return 0;
}


#ifndef XMOJI_UNISTR_H
#define XMOJI_UNISTR_H

#include "macros.h"

#include <poser/decl.h>
#include <uchar.h>

C_CLASS_DECL(PSC_List);

typedef struct UniStr
{
    size_t len;
    char32_t *str;
    int refcnt;
} UniStr;

/* static const initialization
 *
 * USAGE: UniStr(name, "value");
 * declares 'static const *UniStr name' and initializes it to "value".
 */

#define priv_UniStrLit(n,x) static const UniStr n ## _v = { \
	.len = (sizeof x >> 2) - 1, \
	.str = (char32_t *)x, \
	.refcnt = -1 }; \
	static const UniStr *n = & n ## _v
#define UniStr(n,x) priv_UniStrLit(n,U32LIT(x))

/* constructor */

#define UniStr_create(x) _Generic(x, \
	char *: UniStr_createFromUtf8, \
	const char *: UniStr_createFromUtf8, \
	char32_t *: UniStr_createOwned, \
	const char32_t *: UniStr_createFromUtf32)(x)
UniStr *UniStr_createFromUtf8(const char *utf8)
    ATTR_NONNULL((1)) ATTR_RETNONNULL;
UniStr *UniStr_createFromUtf32(const char32_t *utf32)
    ATTR_NONNULL((1)) ATTR_RETNONNULL;
UniStr *UniStr_createOwned(char32_t *utf32)
    ATTR_NONNULL((1)) ATTR_RETNONNULL;

/* reference */

UniStr *UniStr_ref(const UniStr *self)
    CMETHOD ATTR_RETNONNULL;

/* getters */

size_t UniStr_len(const UniStr *self)
    CMETHOD;
const char32_t *UniStr_str(const UniStr *self)
    CMETHOD ATTR_RETNONNULL;

/* mutators, ceating new instances */

#define UniStr_append(s,x) _Generic(&x, \
	U8LPT(x): UniStr_appendUtf32, \
	const U8LPT(x): UniStr_appendUtf32, \
	U32LPT(x): UniStr_appendUtf32, \
	const U32LPT(x): UniStr_appendUtf32, \
	char **: UniStr_appendUtf8, \
	const char **: UniStr_appendUtf8, \
	char32_t **: UniStr_appendUtf32, \
	const char32_t **: UniStr_appendUtf32)(s, U32LIT(x))
UniStr *UniStr_appendUtf8(const UniStr *self, const char *utf8)
    CMETHOD ATTR_NONNULL((2)) ATTR_RETNONNULL;
UniStr *UniStr_appendUtf32(const UniStr *self, const char32_t *utf32)
    CMETHOD ATTR_NONNULL((2)) ATTR_RETNONNULL;

#define UniStr_split(s,x) _Generic(&x, \
	U8LPT(x): UniStr_splitByUtf32, \
	const U8LPT(x): UniStr_splitByUtf32, \
	U32LPT(x): UniStr_splitByUtf32, \
	const U32LPT(x): UniStr_splitByUtf32, \
	char **: UniStr_splitByUtf8, \
	const char **: UniStr_splitByUtf8, \
	char32_t **: UniStr_splitByUtf32, \
	const char32_t **: UniStr_splitByUtf32)(s, U32LIT(x))
PSC_List *UniStr_splitByUtf8(const UniStr *self, const char *delim)
    CMETHOD ATTR_NONNULL((2)) ATTR_RETNONNULL;
PSC_List *UniStr_splitByUtf32(const UniStr *self, const char32_t *delim)
    CMETHOD ATTR_NONNULL((2)) ATTR_RETNONNULL;

/* destructor */

void UniStr_destroy(UniStr *self);

/* related utility functions */

#define LATIN1(x) _Generic(x, \
	const UniStr *: UniStr_toLatin1, \
	UniStr *: UniStr_toLatin1, \
	const char *: UniStr_utf8ToLatin1, \
	char *: UniStr_utf8ToLatin1 )(x)
char *UniStr_toLatin1(const UniStr *str)
    ATTR_NONNULL((1));
char *UniStr_utf8ToLatin1(const char *utf8);

char *UniStr_toUtf8(const UniStr *str, size_t *len)
    ATTR_NONNULL((1));

size_t UniStr_utf32len(const char32_t *s)
    ATTR_NONNULL((1));

#endif

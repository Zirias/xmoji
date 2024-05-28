#ifndef XMOJI_UNISTR_H
#define XMOJI_UNISTR_H

#include <poser/decl.h>
#include <uchar.h>

C_CLASS_DECL(UniStr);

#define priv_USCreate(x) _Generic(x, \
	const char *: UniStr_fromUtf8, \
	char *: UniStr_fromUtf8, \
	const char32_t *: UniStr_fromUtf32, \
	char32_t *: UniStr_fromUtf32 \
)(x)
#define UniStr_create(...) _Generic(&#__VA_ARGS__, \
	char(*)[1]: UniStr_createEmpty(), \
	default: priv_USCreate(__VA_ARGS__) \
)

UniStr *UniStr_createEmpty(void)
    ATTR_RETNONNULL;
UniStr *UniStr_fromUtf8(const char *utf8)
    ATTR_NONNULL((1)) ATTR_RETNONNULL;
UniStr *UniStr_fromUtf32(const char32_t *utf32)
    ATTR_NONNULL((1)) ATTR_RETNONNULL;

void UniStr_appendUtf8(UniStr *self, const char *utf8)
    CMETHOD ATTR_NONNULL((2));
void UniStr_appendUtf32(UniStr *self, const char32_t *utf32)
    CMETHOD ATTR_NONNULL((2));

size_t UniStr_utf8len(UniStr *self)
    CMETHOD;
const char *UniStr_utf8(UniStr *self)
    CMETHOD ATTR_RETNONNULL;
size_t UniStr_utf32len(UniStr *self)
    CMETHOD;
const char32_t *UniStr_utf32(UniStr *self)
    CMETHOD ATTR_RETNONNULL;

char *UniStr_toLatin1(UniStr *self)
    ATTR_NONNULL((1)) ATTR_MALLOC;

void UniStr_destroy(UniStr *self);

char *UniStr_utf8ToLatin1(const char *utf8)
    ATTR_MALLOC;

#define LATIN1(x) _Generic(x, \
	UniStr *: UniStr_toLatin1, \
	const char *: UniStr_utf8ToLatin1, \
	char *: UniStr_utf8ToLatin1 \
)(x)

#endif

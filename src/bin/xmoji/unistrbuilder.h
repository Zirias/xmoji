#ifndef XMOJI_UNISTRBUILDER_H
#define XMOJI_UNISTRBUILDER_H

#include <poser/decl.h>
#include <uchar.h>

C_CLASS_DECL(UniStr);
C_CLASS_DECL(UniStrBuilder);

UniStrBuilder *UniStrBuilder_create(void);
UniStrBuilder *UniStrBuilder_clone(UniStrBuilder *builder);

void UniStrBuilder_appendChar(UniStrBuilder *self, char32_t c)
    CMETHOD;
void UniStrBuilder_appendStr(UniStrBuilder *self, const char32_t *s)
    CMETHOD ATTR_NONNULL((2));

void UniStrBuilder_insertChar(UniStrBuilder *self,
	size_t pos, char32_t c)
    CMETHOD;
void UniStrBuilder_insertStr(UniStrBuilder *self,
	size_t pos, const char32_t *s, size_t maxlen)
    CMETHOD ATTR_NONNULL((3));

void UniStrBuilder_clear(UniStrBuilder *self)
    CMETHOD;
void UniStrBuilder_remove(UniStrBuilder *self,
	size_t pos, size_t len)
    CMETHOD;

UniStr *UniStrBuilder_string(const UniStrBuilder *self)
    CMETHOD ATTR_RETNONNULL;
const UniStr *UniStrBuilder_stringView(const UniStrBuilder *self)
    CMETHOD ATTR_RETNONNULL;

void UniStrBuilder_destroy(UniStrBuilder *self);

#endif

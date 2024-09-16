#ifndef XMOJI_TRANSLATOR_H
#define XMOJI_TRANSLATOR_H

#include <poser/decl.h>

C_CLASS_DECL(Translator);

#define TR Translator_getTranslation
#define NTR Translator_getOriginal

Translator *Translator_create(const char *name, const char *lang,
	const void *(*gettext)(unsigned id))
    ATTR_NONNULL((1)) ATTR_NONNULL((2)) ATTR_NONNULL((3)) ATTR_RETNONNULL;
const void *Translator_getTranslation(const Translator *self, unsigned id)
    CMETHOD;
const void *Translator_getOriginal(const Translator *self, unsigned id)
    CMETHOD;
void Translator_destroy(Translator *self);

#endif

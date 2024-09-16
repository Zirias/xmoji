#include "translator.h"

#include "unistr.h"

#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_NLS
typedef struct TranslationEntry
{
    void *str;
    int type;
} TranslationEntry;
#endif

struct Translator
{
    const void *(*gettext)(unsigned id);
#ifdef WITH_NLS
    TranslationEntry *translations;
    unsigned translationslen;
#endif
};

#ifdef WITH_NLS
static int read32le(unsigned *val, FILE *in)
{
    unsigned char data[4];
    if (fread(data, 4, 1, in) != 1) return -1;
    *val = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
    return 0;
}

static char *xctname(const char *name, const char *lang)
{
    size_t nmlen = strlen(name);
    size_t langlen = strcspn(lang, "_.@");

    size_t xctlen = sizeof TRANSDIR + nmlen + langlen + 5;
    char *xctnm = PSC_malloc(xctlen+1);
    memcpy(xctnm, TRANSDIR, sizeof TRANSDIR - 1);
    xctnm[sizeof TRANSDIR - 1] = '/';
    memcpy(xctnm + sizeof TRANSDIR, name, nmlen);
    xctnm[sizeof TRANSDIR + nmlen] = '-';
    memcpy(xctnm + sizeof TRANSDIR + nmlen + 1, lang, langlen);
    memcpy(xctnm + sizeof TRANSDIR + nmlen + langlen + 1,
	    ".xct", sizeof ".xct");
    return xctnm;
}

static void loadTranslations(Translator *self,
	const char *name, const char *lang)
{
    self->translations = 0;
    self->translationslen = 0;
    char *val = 0;
    UniStr *us = 0;
    char *xctnm = xctname(name, lang);
    FILE *xct = fopen(xctnm, "rb");
    if (!xct) goto done;
    char magic[4];
    if (fread(magic, 4, 1, xct) != 1) goto done;
    unsigned len;
    if (read32le(&len, xct) < 0) goto done;
    self->translations = PSC_malloc(len * sizeof *self->translations);
    memset(self->translations, 0, len * sizeof *self->translations);
    self->translationslen = len;
    for (unsigned i = 0; i < len; ++i)
    {
	if ((self->translations[i].type = fgetc(xct)) < 0) continue;
	unsigned slen;
	if (read32le(&slen, xct) < 0) goto done;
	val = PSC_malloc(slen+1);
	if (fread(val, 1, slen, xct) < slen) goto done;
	val[slen] = 0;
	switch (self->translations[i].type)
	{
	    case 0:
		self->translations[i].str = val;
		val = 0;
		break;
	    case 1:
		us = UniStr_create(val);
		self->translations[i].str = us;
		free(val);
		val = 0;
		break;
	    default:
		self->translations[i].type = -1;
		goto done;
	}
    }

done:
    if (xct) fclose(xct);
    free(val);
    free(xctnm);
}
#endif

Translator *Translator_create(const char *name, const char *lang,
	const void *(*gettext)(unsigned id))
{
    Translator *self = PSC_malloc(sizeof *self);
    self->gettext = gettext;
#ifdef WITH_NLS
    loadTranslations(self, name, lang);
#else
    (void)name;
    (void)lang;
#endif
    return self;
}

const void *Translator_getTranslation(const Translator *self, unsigned id)
{
#ifdef WITH_NLS
    if (self->translations && id < self->translationslen
	    && self->translations[id].str)
    {
	return self->translations[id].str;
    }
#endif
    return self->gettext(id);
}

const void *Translator_getOriginal(const Translator *self, unsigned id)
{
    return self->gettext(id);
}

void Translator_destroy(Translator *self)
{
#ifdef WITH_NLS
    if (!self) return;
    if (self->translations)
    {
	for (unsigned i = 0; i < self->translationslen; ++i)
	{
	    if (self->translations[i].type < 0) continue;
	    if (self->translations[i].type == 1)
	    {
		UniStr_destroy(self->translations[i].str);
	    }
	    else free(self->translations[i].str);
	}
	free(self->translations);
    }
#endif
    free(self);
}


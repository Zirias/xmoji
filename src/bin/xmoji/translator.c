#include "translator.h"

#include <poser/core.h>
#include <stdlib.h>

struct Translator
{
    const void *(*gettext)(unsigned id);
};

Translator *Translator_create(const char *name, const char *basepath,
	const void *(*gettext)(unsigned id))
{
    (void)name;
    (void)basepath;

    Translator *self = PSC_malloc(sizeof *self);
    self->gettext = gettext;
    return self;
}

const void *Translator_getTranslation(const Translator *self, unsigned id)
{
    return self->gettext(id);
}

const void *Translator_getOriginal(const Translator *self, unsigned id)
{
    return self->gettext(id);
}

void Translator_destroy(Translator *self)
{
    free(self);
}


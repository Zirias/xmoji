#ifndef XMOJI_FONT_H
#define XMOJI_FONT_H

#include <poser/decl.h>

C_CLASS_DECL(Font);

Font *Font_create(char **patterns);
void Font_destroy(Font *self);

#endif

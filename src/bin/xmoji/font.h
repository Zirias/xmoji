#ifndef XMOJI_FONT_H
#define XMOJI_FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <poser/decl.h>

C_CLASS_DECL(Font);

int Font_init(void);
void Font_done(void);

Font *Font_create(char **patterns);
FT_Face Font_face(const Font *self) CMETHOD ATTR_RETNONNULL;
void Font_destroy(Font *self);

#endif

#ifndef XMOJI_FONT_H
#define XMOJI_FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);

int Font_init(void);
void Font_done(void);

Font *Font_create(char **patterns);
FT_Face Font_face(const Font *self) CMETHOD ATTR_RETNONNULL;
double Font_pixelsize(const Font *self) CMETHOD;
int Font_uploadGlyph(Font *self, uint32_t glyphid) CMETHOD;
xcb_render_glyphset_t Font_glyphset(const Font *self) CMETHOD;
void Font_destroy(Font *self);

#endif

#ifndef XMOJI_FONT_H
#define XMOJI_FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);

typedef struct GlyphRenderInfo
{
    uint8_t count;
    uint8_t pad[3];
    int16_t dx;
    int16_t dy;
    uint32_t glyphid;
} GlyphRenderInfo;

int Font_init(void);
void Font_done(void);

Font *Font_create(uint8_t subpixelbits, char **patterns);
FT_Face Font_face(const Font *self) CMETHOD ATTR_RETNONNULL;
double Font_pixelsize(const Font *self) CMETHOD;
uint8_t Font_glyphidbits(const Font *self) CMETHOD;
uint8_t Font_subpixelbits(const Font *self) CMETHOD;
int Font_uploadGlyphs(Font *self, unsigned len, GlyphRenderInfo *glyphinfo)
    CMETHOD ATTR_NONNULL((3));
xcb_render_glyphset_t Font_glyphset(const Font *self) CMETHOD;
void Font_destroy(Font *self);

#endif

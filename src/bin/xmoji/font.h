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

typedef enum FontGlyphType
{
    FGT_OUTLINE,
    FGT_BITMAP_GRAY,
    FGT_BITMAP_BGRA
} FontGlyphType;

typedef struct FontOptions
{
    float maxUnscaledDeviation;
    uint8_t pixelFractionBits;
} FontOptions;

Font *Font_create(const char *pattern, const FontOptions *options);
Font *Font_ref(Font *font);
FT_Face Font_face(const Font *self) CMETHOD ATTR_RETNONNULL;
FontGlyphType Font_glyphtype(const Font *self) CMETHOD;
double Font_pixelsize(const Font *self) CMETHOD;
double Font_fixedpixelsize(const Font *self) CMETHOD;
uint8_t Font_glyphidbits(const Font *self) CMETHOD;
uint8_t Font_subpixelbits(const Font *self) CMETHOD;
uint16_t Font_linespace(const Font *self) CMETHOD;
uint32_t Font_maxWidth(const Font *self) CMETHOD;
uint32_t Font_maxHeight(const Font *self) CMETHOD;
uint32_t Font_baseline(const Font *self) CMETHOD;
uint32_t Font_scale(const Font *self, uint32_t val) CMETHOD;
int32_t Font_ftLoadFlags(const Font *self) CMETHOD;
int Font_uploadGlyphs(Font *self, uint32_t ownerid,
	unsigned len, GlyphRenderInfo *glyphinfo)
    CMETHOD ATTR_NONNULL((4));
xcb_render_glyphset_t Font_glyphset(const Font *self) CMETHOD;
xcb_render_glyphset_t Font_maskGlyphset(const Font *self) CMETHOD;
void Font_destroy(Font *self);

#endif

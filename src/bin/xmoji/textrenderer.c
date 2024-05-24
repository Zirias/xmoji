#include "textrenderer.h"

#include "font.h"
#include "x11adapter.h"

#include <hb.h>
#include <hb-ft.h>
#include <poser/core.h>
#include <string.h>

struct TextRenderer
{
    Font *font;
    hb_font_t *hbfont;
    hb_buffer_t *hbbuffer;
    xcb_pixmap_t pixmap;
    xcb_pixmap_t tmp;
    xcb_render_picture_t pen;
    xcb_render_picture_t tpic;
    Color color;
    Size size;
    int haserror;
};

static void requestError(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    TextRenderer *self = receiver;
    PSC_Log_fmt(PSC_L_ERROR, "TextRenderer: Failed for pixmap 0x%x",
	    (unsigned)self->pixmap);
    self->haserror = 1;
}

TextRenderer *TextRenderer_create(Font *font)
{
    TextRenderer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->font = font;
    self->hbfont = hb_ft_font_create_referenced(Font_face(font));
    xcb_connection_t *c = X11Adapter_connection();
    self->pixmap = xcb_generate_id(c);
    PSC_Event_register(X11Adapter_requestError(), self, requestError,
	    self->pixmap);
    CHECK(xcb_create_pixmap(c, 32, self->pixmap,
		X11Adapter_screen()->root, 1, 1),
	    "TextRenderer: Cannot create drawing pixmap 0x%x",
	    (unsigned)self->pixmap);
    self->pen = xcb_generate_id(c);
    uint32_t repeat = XCB_RENDER_REPEAT_NORMAL;
    CHECK(xcb_render_create_picture(c, self->pen, self->pixmap,
		X11Adapter_argbformat(), XCB_RENDER_CP_REPEAT, &repeat),
	    "TextRenderer: Cannot create pen on pixmap 0x%x",
	    (unsigned)self->pixmap);
    return self;
}

Size TextRenderer_size(const TextRenderer *self)
{
    return self->size;
}

int TextRenderer_setUtf8(TextRenderer *self, const char *utf8, int len)
{
    if (self->haserror) return -1;
    hb_buffer_destroy(self->hbbuffer);
    self->hbbuffer = hb_buffer_create();
    hb_buffer_add_utf8(self->hbbuffer, utf8, len, 0, -1);
    hb_buffer_set_language(self->hbbuffer, hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(self->hbbuffer);
    hb_shape(self->hbfont, self->hbbuffer, 0, 0);
    unsigned slen = hb_buffer_get_length(self->hbbuffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(self->hbbuffer, 0);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(
	    self->hbbuffer, 0);
    uint32_t width = 0;
    uint32_t height = 0;
    FT_Face face = Font_face(self->font);
    FT_Load_Glyph(face, info[slen-1].codepoint, Font_ftLoadFlags(self->font));
    if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(self->hbbuffer)))
    {
	for (unsigned i = 0; i < slen-1; ++i)
	{
	    width += pos[i].x_advance;
	}
	width += Font_scale(self->font, face->glyph->metrics.horiBearingX
		+ face->glyph->metrics.width);
	height = Font_maxHeight(self->font);
    }
    else
    {
	for (unsigned i = 0; i < slen - 1; ++i)
	{
	    height += pos[i].y_advance;
	}
	height += Font_scale(self->font, face->glyph->metrics.vertBearingY
		+ face->glyph->metrics.height);
	width = Font_maxWidth(self->font);
    }
    self->size.width = (width + 0x3fU) >> 6;
    self->size.height = (height + 0x3fU) >> 6;
    xcb_connection_t *c = X11Adapter_connection();
    if (self->tpic) xcb_render_free_picture(c, self->tpic);
    if (self->tmp) xcb_free_pixmap(c, self->tmp);
    self->tpic = 0;
    self->tmp = 0;
    return 0;
}

int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos)
{
    if (self->haserror || !self->hbbuffer) return -1;
    unsigned len = hb_buffer_get_length(self->hbbuffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(self->hbbuffer, 0);
    hb_glyph_position_t *gpos = hb_buffer_get_glyph_positions(
	    self->hbbuffer, 0);
    GlyphRenderInfo *glyphs = PSC_malloc(len * sizeof *glyphs);
    uint32_t x = 0;
    uint32_t y = Font_baseline(self->font);
    uint32_t rx = 0;
    uint32_t prx = 0;
    uint16_t ry = 0;
    uint16_t pry = 0;
    uint8_t glyphidbits = Font_glyphidbits(self->font);
    uint8_t subpixelbits = Font_subpixelbits(self->font);
    uint8_t roundbits = 6 - subpixelbits;
    uint8_t roundadd = 0;
    if (roundbits) roundadd = 1U << (roundbits - 1);
    uint8_t subpixelmask = (1U << subpixelbits) - 1;
    for (unsigned i = 0; i < len; ++i)
    {
	rx = (x + roundadd) >> roundbits;
	ry = (y + 0x20) >> 6;
	glyphs[i].count = 1;
	glyphs[i].dx = (rx >> subpixelbits) - (prx >> subpixelbits)
	    + ((gpos[i].x_offset + 0x20) >> 6);
	glyphs[i].dy = ry - pry + ((gpos[i].y_offset + 0x20) >> 6);
	glyphs[i].glyphid = info[i].codepoint
	    | ((rx & subpixelmask) << glyphidbits);
	prx = rx;
	pry = ry;
	x += gpos[i].x_advance;
	y += gpos[i].y_advance;
    }
    xcb_connection_t *c = X11Adapter_connection();
    int isColor = Font_glyphtype(self->font) == FGT_BITMAP_BGRA;
    if (isColor) color = 0xffffffff;
    if (color != self->color)
    {
	xcb_rectangle_t rect = { 0, 0, 1, 1 };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    self->pen, Color_xcb(color), 1, &rect),
		"TextRenderer: Cannot colorize pen for 0x%x",
		(unsigned)self->pixmap);
	self->color = color;
    }
    Font_uploadGlyphs(self->font, len, glyphs);
    if (isColor)
    {
	if (!self->tpic)
	{
	    self->tmp = xcb_generate_id(c);
	    CHECK(xcb_create_pixmap(c, 32, self->tmp,
			X11Adapter_screen()->root,
			self->size.width, self->size.height),
		    "TextRenderer: Cannot create temporary pixmap for 0x%x",
		    (unsigned)self->pixmap);
	    self->tpic = xcb_generate_id(c);
	    CHECK(xcb_render_create_picture(c, self->tpic, self->tmp,
			X11Adapter_argbformat(), 0, 0),
		    "TextRenderer: Cannot create temporary picture for 0x%x",
		    (unsigned)self->pixmap);
	    CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_IN,
			self->pen, self->tpic, 0, Font_glyphset(self->font),
			0, 0, len * sizeof *glyphs, (const uint8_t *)glyphs),
		    "TextRenderer: Cannot render glyphs for 0x%x",
		    (unsigned)self->pixmap);
	}
	xcb_render_transform_t shift = {
	    1 << 16, 0, -pos.x << 16,
	    0, 1 << 16, -pos.y << 16,
	    0, 0, 1 << 16
	};
	CHECK(xcb_render_set_picture_transform(c, self->tpic, shift),
		"TextRenderer: Cannot shift temporary picture for 0x%x",
		(unsigned)self->pixmap);
	glyphs[0].dx += pos.x;
	glyphs[0].dy += pos.y;
	CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER,
		    self->tpic, picture, 0, Font_maskGlyphset(self->font),
		    0, 0, len * sizeof *glyphs, (const uint8_t *)glyphs),
		"TextRenderer: Cannot blend glyphs for 0x%x",
		(unsigned)self->pixmap);
    }
    else
    {
	glyphs[0].dx += pos.x;
	glyphs[0].dy += pos.y;
	CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER,
		    self->pen, picture, 0, Font_glyphset(self->font), 0, 0,
		    len * sizeof *glyphs, (const uint8_t *)glyphs),
		"TextRenderer: Cannot render glyphs for 0x%x",
		(unsigned)self->pixmap);
    }
    free(glyphs);
    return 0;
}

void TextRenderer_destroy(TextRenderer *self)
{
    if (!self) return;
    hb_buffer_destroy(self->hbbuffer);
    hb_font_destroy(self->hbfont);
    xcb_connection_t *c = X11Adapter_connection();
    if (self->tpic) xcb_render_free_picture(c, self->tpic);
    xcb_render_free_picture(c, self->pen);
    if (self->tmp) xcb_free_pixmap(c, self->tmp);
    xcb_free_pixmap(c, self->pixmap);
    free(self);
}

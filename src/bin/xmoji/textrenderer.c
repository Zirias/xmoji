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
    PSC_Event *shaped;
    hb_font_t *hbfont;
    hb_buffer_t *hbbuffer;
    xcb_pixmap_t pixmap;
    xcb_render_picture_t pen;
    Color color;
    Size size;
    int haserror;
};

typedef struct ShapeContext
{
    TextRenderer *renderer;
    hb_buffer_t *hbbuffer;
    Size size;
} ShapeContext;

static void doshape(void *ctx)
{
    ShapeContext *sctx = ctx;
    hb_buffer_guess_segment_properties(sctx->hbbuffer);
    hb_shape(sctx->renderer->hbfont, sctx->hbbuffer, 0, 0);
    FT_Face face = hb_ft_font_get_face(sctx->renderer->hbfont);
    unsigned len = hb_buffer_get_length(sctx->hbbuffer);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(
	    sctx->hbbuffer, 0);
    uint32_t width = 0;
    uint32_t height = 0;
    if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(sctx->hbbuffer)))
    {
	for (unsigned i = 0; i < len; ++i)
	{
	    width += pos[i].x_advance;
	}
	height = face->size->metrics.ascender - face->size->metrics.descender;
    }
    else
    {
	for (unsigned i = 0; i < len; ++i)
	{
	    height += pos[i].y_advance;
	}
	width = FT_MulFix(face->bbox.xMax, face->size->metrics.x_scale)
	    - FT_MulFix(face->bbox.xMin, face->size->metrics.x_scale);
    }
    sctx->size.width = (width + 0x3fU) >> 6;
    sctx->size.height = (height + 0x3fU) >> 6;
}

static void shapedone(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    ShapeContext *sctx = receiver;
    TextRenderer *self = sctx->renderer;
    hb_buffer_destroy(self->hbbuffer);
    self->hbbuffer = sctx->hbbuffer;
    self->size = sctx->size;
    free(sctx);
    PSC_Event_raise(self->shaped, 0, 0);
}

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
    self->shaped = PSC_Event_create(self);
    self->hbfont = hb_ft_font_create_referenced(Font_face(font));
    xcb_connection_t *c = X11Adapter_connection();
    self->pixmap = xcb_generate_id(c);
    PSC_Event_register(X11Adapter_requestError(), self, requestError,
	    self->pixmap);
    CHECK(xcb_create_pixmap(c, 32, self->pixmap, X11Adapter_screen()->root,
		1, 1),
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

PSC_Event *TextRenderer_shaped(TextRenderer *self)
{
    return self->shaped;
}

Size TextRenderer_size(const TextRenderer *self)
{
    return self->size;
}

int TextRenderer_setUtf8(TextRenderer *self, const char *utf8)
{
    if (self->haserror) return -1;
    ShapeContext *sctx = PSC_malloc(sizeof *sctx);
    sctx->renderer = self;
    sctx->hbbuffer = hb_buffer_create();
    hb_buffer_add_utf8(sctx->hbbuffer, utf8, -1, 0, -1);
    PSC_ThreadJob *shapejob = PSC_ThreadJob_create(doshape, sctx, 0);
    PSC_Event_register(PSC_ThreadJob_finished(shapejob), sctx, shapedone, 0);
    PSC_ThreadPool_enqueue(shapejob);
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
    uint32_t y = (Font_pixelsize(self->font) + .5) * 64.;
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
    glyphs[0].dx += pos.x;
    glyphs[0].dy += pos.y;
    xcb_connection_t *c = X11Adapter_connection();
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
    CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER,
		self->pen, picture, 0, Font_glyphset(self->font), 0, 0,
		len * sizeof *glyphs, (const uint8_t *)glyphs),
	    "TextRenderer: Cannot render glyphs for 0x%x",
	    (unsigned)self->pixmap);
    free(glyphs);
    return 0;
}

void TextRenderer_destroy(TextRenderer *self)
{
    if (!self) return;
    hb_buffer_destroy(self->hbbuffer);
    hb_font_destroy(self->hbfont);
    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_free_picture(c, self->pen);
    xcb_free_pixmap(c, self->pixmap);
    PSC_Event_destroy(self->shaped);
    free(self);
}

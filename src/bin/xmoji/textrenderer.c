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
    PSC_ThreadJob *shapejob;
    void *shapejobctx;
    TR_size_cb sizecb;
    xcb_render_color_t color;
    xcb_pixmap_t pixmap;
    xcb_render_picture_t pen;
    int pencolored;
    int haserror;
    int shaped;
    uint32_t width;
    uint32_t height;
};

typedef struct RenderContext
{
    TextRenderer *renderer;
    xcb_render_picture_t pic;
    unsigned len;
    GlyphRenderInfo glyphs[];
} RenderContext;

static void doshape(void *tr)
{
    TextRenderer *self = tr;
    hb_buffer_guess_segment_properties(self->hbbuffer);
    hb_shape(self->hbfont, self->hbbuffer, 0, 0);
    self->shaped = 1;
}

static void dosetsize(void *tr)
{
    TextRenderer *self = tr;
    FT_Face face = hb_ft_font_get_face(self->hbfont);
    unsigned len = hb_buffer_get_length(self->hbbuffer);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(
	    self->hbbuffer, 0);
    uint32_t width = 0;
    uint32_t height = 0;
    if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(self->hbbuffer)))
    {
	for (unsigned i = 0; i < len; ++i)
	{
	    width += pos[i].x_advance;
	}
	height = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale)
	    - FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale);
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
    self->width = (width + 0x3fU) >> 6;
    self->height = (height + 0x3fU) >> 6;
}

static void getsizejob(void *tr)
{
    TextRenderer *self = tr;
    if (!self->shaped) doshape(self);
    if (!self->width && !self->height) dosetsize(self);
}

static void getsizedone(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    TextRenderer *self = receiver;
    if (!PSC_ThreadJob_hasCompleted(self->shapejob))
    {
	self->width = 0;
	self->height = 0;
    }
    self->sizecb(self->shapejobctx, self->width, self->height);
    self->shapejob = 0;
    self->shapejobctx = 0;
    self->sizecb = 0;
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

TextRenderer *TextRenderer_fromUtf8(Font *font, const char *utf8)
{
    TextRenderer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->font = font;
    self->hbfont = hb_ft_font_create_referenced(Font_face(font));
    self->hbbuffer = hb_buffer_create();
    hb_buffer_add_utf8(self->hbbuffer, utf8, -1, 0, -1);
    self->color.alpha = 0xffff;
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

int TextRenderer_size(TextRenderer *self, void *ctx, TR_size_cb cb)
{
    if (self->width || self->height)
    {
	cb(ctx, self->width, self->height);
	return 0;
    }
    if (self->shapejob) return -1;
    self->shapejob = PSC_ThreadJob_create(getsizejob, self, 0);
    self->shapejobctx = ctx;
    self->sizecb = cb;
    PSC_Event_register(PSC_ThreadJob_finished(self->shapejob), self,
	    getsizedone, 0);
    PSC_ThreadPool_enqueue(self->shapejob);
    return 0;
}

static void dorender(void *ctx)
{
    RenderContext *rctx = ctx;
    xcb_connection_t *c = X11Adapter_connection();
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(
	    rctx->renderer->hbbuffer, 0);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(
	    rctx->renderer->hbbuffer, 0);
    uint32_t x = 0;
    uint32_t y = (Font_pixelsize(rctx->renderer->font) + .5) * 64.;
    uint16_t rx = 0;
    uint16_t ry = 0;
    uint16_t prx = 0;
    uint16_t pry = 0;
    for (unsigned i = 0; i < rctx->len; ++i)
    {
	rx = (x + 8) >> 4;
	ry = (y + 0x20) >> 6;
	rctx->glyphs[i].count = 1;
	rctx->glyphs[i].dx = (rx >> 2) - (prx >> 2)
	    + ((pos[i].x_offset + 0x20) >> 6);
	rctx->glyphs[i].dy = ry - pry + ((pos[i].y_offset + 0x20) >> 6);
	rctx->glyphs[i].glyphid = info[i].codepoint | ((rx & 3) << 16);
	prx = rx;
	pry = ry;
	x += pos[i].x_advance;
	y += pos[i].y_advance;
    }
    if (!rctx->renderer->pencolored)
    {
	xcb_rectangle_t rect = { 0, 0, 1, 1 };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    rctx->renderer->pen, rctx->renderer->color, 1, &rect),
		"TextRenderer: Cannot colorize pen for 0x%x",
		(unsigned)rctx->renderer->pixmap);
	rctx->renderer->pencolored = 1;
    }
    Font_uploadGlyphs(rctx->renderer->font, rctx->len, rctx->glyphs);
    CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER,
		rctx->renderer->pen, rctx->pic, 0,
		Font_glyphset(rctx->renderer->font), 0, 0,
		rctx->len * sizeof *rctx->glyphs,
		(const uint8_t *)rctx->glyphs),
	    "TextRenderer: Cannot render glyphs for 0x%x",
	    (unsigned)rctx->renderer->pixmap);
    free(rctx);
}

static void doshapedone(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    RenderContext *rctx = receiver;
    dorender(rctx);
}

xcb_render_picture_t TextRenderer_createPicture(
	TextRenderer *self, xcb_drawable_t drawable)
{
    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_picture_t pic = xcb_generate_id(c);
    uint32_t values[] = {
	XCB_RENDER_POLY_MODE_IMPRECISE,
	XCB_RENDER_POLY_EDGE_SMOOTH
    };
    CHECK(xcb_render_create_picture(c, pic, drawable, X11Adapter_rootformat(),
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE, values),
	    "TextRenderer: Cannot create XRender picture for 0x%x",
	    (unsigned)self->pixmap);
    return pic;
}

void TextRenderer_render(TextRenderer *self, xcb_render_picture_t picture)
{
    unsigned len = hb_buffer_get_length(self->hbbuffer);
    RenderContext *rctx = PSC_malloc(sizeof *rctx
	    + len * sizeof *rctx->glyphs);
    memset(rctx, 0, sizeof *rctx);
    rctx->renderer = self;
    rctx->pic = picture;
    rctx->len = len;

    if (self->shaped) dorender(rctx);
    else
    {
	PSC_ThreadJob *shapejob = PSC_ThreadJob_create(doshape, self, 0);
	PSC_Event_register(PSC_ThreadJob_finished(shapejob), rctx,
		doshapedone, 0);
	PSC_ThreadPool_enqueue(shapejob);
    }
}

void TextRenderer_destroy(TextRenderer *self)
{
    if (!self) return;
    hb_buffer_destroy(self->hbbuffer);
    hb_font_destroy(self->hbfont);
    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_free_picture(c, self->pen);
    xcb_free_pixmap(c, self->pixmap);
    free(self);
}

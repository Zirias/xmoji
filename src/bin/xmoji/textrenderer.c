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
    int shaped;
    uint32_t width;
    uint32_t height;
};

typedef struct RenderContext
{
    TextRenderer *renderer;
    void *cbctx;
    TR_render_cb cb;
    xcb_drawable_t drawable;
    xcb_pixmap_t src;
    xcb_render_picture_t pic;
    xcb_render_picture_t srcpic;
    int awaiting;
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

TextRenderer *TextRenderer_fromUtf8(Font *font, const char *utf8)
{
    TextRenderer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->font = font;
    self->hbfont = hb_ft_font_create_referenced(Font_face(font));
    self->hbbuffer = hb_buffer_create();
    hb_buffer_add_utf8(self->hbbuffer, utf8, -1, 0, -1);
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

static void dorender(void *ctx, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    RenderContext *rctx = ctx;
    xcb_connection_t *c = X11Adapter_connection();
    if (rctx->awaiting)
    {
	if (error)
	{
	    PSC_Log_msg(PSC_L_ERROR, "Could not render text.");
	}
	if (!--rctx->awaiting)
	{
	    xcb_render_free_picture(c, rctx->srcpic);
	    xcb_render_free_picture(c, rctx->pic);
	    xcb_free_pixmap(c, rctx->src);
	    free(rctx);
	}
	return;
    }
    unsigned len = hb_buffer_get_length(rctx->renderer->hbbuffer);
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(
	    rctx->renderer->hbbuffer, 0);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(
	    rctx->renderer->hbbuffer, 0);
    struct glyphitem {
	uint8_t count;
	uint8_t pad0[3];
	int16_t dx;
	int16_t dy;
	uint16_t glyphid;
	uint8_t pad1[2];
    } *glyphs = PSC_malloc(len * sizeof *glyphs);
    memset(glyphs, 0, len * sizeof *glyphs);
    uint32_t x = 0;
    uint32_t y = (Font_pixelsize(rctx->renderer->font) + .5) * 64.;
    uint16_t rx = 0;
    uint16_t ry = 0;
    uint16_t prx = 0;
    uint16_t pry = 0;
    for (unsigned i = 0; i < len; ++i)
    {
	rx = (x + 0x20) >> 6;
	ry = (y + 0x20) >> 6;
	Font_uploadGlyph(rctx->renderer->font, info[i].codepoint);
	glyphs[i].count = 1;
	glyphs[i].dx = rx - prx + ((pos[i].x_offset + 0x20) >> 6);
	glyphs[i].dy = ry - pry + ((pos[i].y_offset + 0x20) >> 6);
	glyphs[i].glyphid = info[i].codepoint;
	prx = rx;
	pry = ry;
	x += pos[i].x_advance;
	y += pos[i].y_advance;
    }
    rctx->src = xcb_generate_id(c);
    AWAIT(xcb_create_pixmap(c, 32, rctx->src,
		X11Adapter_screen()->root, 1, 1),
	    rctx, dorender);
    ++rctx->awaiting;
    rctx->pic = xcb_generate_id(c);
    uint32_t values[] = {
	XCB_RENDER_POLY_MODE_IMPRECISE,
	XCB_RENDER_POLY_EDGE_SMOOTH
    };
    AWAIT(xcb_render_create_picture(c, rctx->pic, rctx->drawable,
		X11Adapter_rootformat(),
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE, values),
	    rctx, dorender);
    ++rctx->awaiting;
    rctx->srcpic = xcb_generate_id(c);
    values[0] = XCB_RENDER_REPEAT_NORMAL;
    AWAIT(xcb_render_create_picture(c, rctx->srcpic, rctx->src,
		X11Adapter_argbformat(), XCB_RENDER_CP_REPEAT, values),
	    rctx, dorender);
    ++rctx->awaiting;
    xcb_render_color_t black = { 0xffff, 0, 0, 0 };
    xcb_rectangle_t rect = { 0, 0, 1, 1 };
    AWAIT(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		rctx->srcpic, black, 1, &rect),
	    rctx, dorender);
    ++rctx->awaiting;
    AWAIT(xcb_render_composite_glyphs_16(c, XCB_RENDER_PICT_OP_OVER,
		rctx->srcpic, rctx->pic, 0,
		Font_glyphset(rctx->renderer->font), 0, 0,
		len * sizeof *glyphs, (const uint8_t *)glyphs),
	    rctx, dorender);
    ++rctx->awaiting;
    free(glyphs);
}

static void doshapedone(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    RenderContext *rctx = receiver;
    dorender(rctx, 0, 0, 0);
}

void TextRenderer_render(TextRenderer *self, xcb_drawable_t drawable,
	void *ctx, TR_render_cb cb)
{
    RenderContext *rctx = PSC_malloc(sizeof *rctx);
    memset(rctx, 0, sizeof *rctx);
    rctx->renderer = self;
    rctx->cbctx = ctx;
    rctx->cb = cb;
    rctx->drawable = drawable;

    if (self->shaped) dorender(rctx, 0, 0, 0);
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
    free(self);
}

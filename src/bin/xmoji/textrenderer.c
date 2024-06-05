#include "textrenderer.h"

#include "font.h"
#include "unistr.h"
#include "x11adapter.h"

#include <hb.h>
#include <hb-ft.h>
#include <poser/core.h>
#include <string.h>

static const hb_feature_t nolig = {
    .tag = HB_TAG('l','i','g','a'),
    .value = 0,
    .start = HB_FEATURE_GLOBAL_START,
    .end = HB_FEATURE_GLOBAL_END
};

struct TextRenderer
{
    Font *font;
    hb_font_t *hbfont;
    hb_buffer_t *hbbuffer;
    hb_glyph_info_t *hbglyphs;
    hb_glyph_position_t *hbpos;
    GlyphRenderInfo *glyphs;
    xcb_pixmap_t pixmap;
    xcb_pixmap_t tmp;
    xcb_render_picture_t pen;
    xcb_render_picture_t tpic;
    Color color;
    Color tcolor;
    Color scolor;
    Size size;
    Pos pos;
    Selection selection;
    unsigned hblen;
    int haserror;
    int noligatures;
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
    CHECK(xcb_create_pixmap(c, 24, self->pixmap,
		X11Adapter_screen()->root, 1, 1),
	    "TextRenderer: Cannot create drawing pixmap 0x%x",
	    (unsigned)self->pixmap);
    self->pen = xcb_generate_id(c);
    uint32_t repeat = XCB_RENDER_REPEAT_NORMAL;
    CHECK(xcb_render_create_picture(c, self->pen, self->pixmap,
		X11Adapter_rgbformat(), XCB_RENDER_CP_REPEAT, &repeat),
	    "TextRenderer: Cannot create pen on pixmap 0x%x",
	    (unsigned)self->pixmap);
    if (Font_glyphtype(self->font) == FGT_BITMAP_BGRA)
    {
	self->color = 0xffffffff;
	xcb_rectangle_t rect = { 0, 0, 1, 1 };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    self->pen, Color_xcb(self->color), 1, &rect),
		"TextRenderer: Cannot colorize pen for 0x%x",
		(unsigned)self->pixmap);
    }
    return self;
}

Size TextRenderer_size(const TextRenderer *self)
{
    return self->size;
}

void TextRenderer_setNoLigatures(TextRenderer *self, int noLigatures)
{
    self->noligatures = !!noLigatures;
}

static void createTmpPicture(TextRenderer *self, xcb_connection_t *c)
{
    self->tmp = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 24, self->tmp, X11Adapter_screen()->root,
		self->size.width, self->size.height),
	    "TextRenderer: Cannot create temporary pixmap for 0x%x",
	    (unsigned)self->pixmap);
    self->tpic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, self->tpic, self->tmp,
		X11Adapter_rgbformat(), 0, 0),
	    "TextRenderer: Cannot create temporary picture for 0x%x",
	    (unsigned)self->pixmap);
}

int TextRenderer_setText(TextRenderer *self, const UniStr *text)
{
    if (self->haserror) return -1;
    hb_buffer_destroy(self->hbbuffer);
    unsigned len = UniStr_len(text);
    if (!len)
    {
	self->hbbuffer = 0;
	self->hbglyphs = 0;
	self->hbpos = 0;
	self->size = (Size){0, 0};
	self->hblen = 0;
	return 0;
    }
    self->hbbuffer = hb_buffer_create();
    hb_buffer_add_codepoints(self->hbbuffer, UniStr_str(text), len, 0, -1);
    hb_buffer_set_language(self->hbbuffer, hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(self->hbbuffer);
    if (self->noligatures) hb_shape(self->hbfont, self->hbbuffer, &nolig, 1);
    else hb_shape(self->hbfont, self->hbbuffer, 0, 0);
    self->hblen = hb_buffer_get_length(self->hbbuffer);
    self->hbglyphs = hb_buffer_get_glyph_infos(self->hbbuffer, 0);
    self->hbpos = hb_buffer_get_glyph_positions(self->hbbuffer, 0);
    uint32_t width = 0;
    uint32_t height = 0;
    FT_Face face = Font_face(self->font);
    FT_Load_Glyph(face, self->hbglyphs[self->hblen-1].codepoint,
	    Font_ftLoadFlags(self->font));
    if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(self->hbbuffer)))
    {
	for (unsigned i = 0; i < self->hblen - 1; ++i)
	{
	    width += self->hbpos[i].x_advance;
	}
	width += Font_scale(self->font, face->glyph->metrics.horiBearingX
		+ face->glyph->metrics.width);
	height = Font_maxHeight(self->font);
    }
    else
    {
	for (unsigned i = 0; i < self->hblen - 1; ++i)
	{
	    height += self->hbpos[i].y_advance;
	}
	height += Font_scale(self->font, face->glyph->metrics.vertBearingY
		+ face->glyph->metrics.height);
	width = Font_maxWidth(self->font);
    }
    self->size.width = (width + 0x3fU) >> 6;
    self->size.height = (height + 0x3fU) >> 6;
    xcb_connection_t *c = X11Adapter_connection();
    if (self->tpic)
    {
	xcb_render_free_picture(c, self->tpic);
	xcb_free_pixmap(c, self->tmp);
	self->tpic = 0;
	self->tmp = 0;
    }
    free(self->glyphs);
    self->glyphs = PSC_malloc(self->hblen * sizeof *self->glyphs);
    memset(self->glyphs, 0, self->hblen * sizeof *self->glyphs);
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
    for (unsigned i = 0; i < self->hblen; ++i)
    {
	rx = (x + roundadd) >> roundbits;
	ry = (y + 0x20) >> 6;
	self->glyphs[i].count = 1;
	self->glyphs[i].dx = (rx >> subpixelbits) - (prx >> subpixelbits)
	    + ((self->hbpos[i].x_offset + 0x20) >> 6);
	self->glyphs[i].dy = ry - pry
	    + ((self->hbpos[i].y_offset + 0x20) >> 6);
	self->glyphs[i].glyphid = self->hbglyphs[i].codepoint
	    | ((rx & subpixelmask) << glyphidbits);
	prx = rx;
	pry = ry;
	x += self->hbpos[i].x_advance;
	y += self->hbpos[i].y_advance;
    }
    Font_uploadGlyphs(self->font, self->hblen, self->glyphs);
    if (Font_glyphtype(self->font) == FGT_BITMAP_BGRA)
    {
	createTmpPicture(self, c);
	CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_IN,
		    self->pen, self->tpic, 0, Font_glyphset(self->font), 0, 0,
		    self->hblen * sizeof *self->glyphs,
		    (const uint8_t *)self->glyphs),
		"TextRenderer: Cannot render glyphs for 0x%x",
		(unsigned)self->pixmap);
    }
    return 0;
}

unsigned TextRenderer_glyphLen(const TextRenderer *self, unsigned index)
{
    unsigned pos = 0;
    if (!self->hblen) return 0;
    for (; pos < self->hblen && self->hbglyphs[pos].cluster < index; ++pos);
    if (!pos) return self->hbglyphs[0].cluster;
    if (pos == self->hblen) return index - self->hbglyphs[pos-1].cluster;
    return self->hbglyphs[pos].cluster - self->hbglyphs[pos-1].cluster;
}

unsigned TextRenderer_pixelOffset(const TextRenderer *self, unsigned index)
{
    uint32_t offset = 0;
    for (unsigned i = 0; i < self->hblen; ++i)
    {
	if (self->hbglyphs[i].cluster >= index) break;
	offset += self->hbpos[i].x_advance;
    }
    return (offset + 0x20) >> 6;
}

int TextRenderer_renderWithSelection(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos,
	Selection selection, Color selectionColor)
{
    if (self->haserror || !self->hbbuffer) return -1;
    uint16_t odx = self->glyphs[0].dx;
    uint16_t ody = self->glyphs[0].dy;
    self->glyphs[0].dx += pos.x;
    self->glyphs[0].dy += pos.y;
    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_picture_t srcpic = self->pen;
    if (selection.len)
    {
	if (!self->tpic) createTmpPicture(self, c);
	if (memcmp(&selection, &self->selection, sizeof selection)
		|| color != self->tcolor
		|| selectionColor != self->scolor)
	{
	    xcb_rectangle_t rect = { 0, 0,
		self->size.width, self->size.height };
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			self->tpic, Color_xcb(color), 1, &rect),
		    "TextRenderer: Cannot colorize foreground for 0x%x",
		    (unsigned)self->pixmap);
	    rect.x = selection.start;
	    rect.width = selection.len;
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			self->tpic, Color_xcb(selectionColor), 1, &rect),
		    "TextRenderer: Cannot colorize selection for 0x%x",
		    (unsigned)self->pixmap);
	    self->scolor = selectionColor;
	    self->tcolor = color;
	    self->selection = selection;
	}
    }
    if (selection.len || Font_glyphtype(self->font) == FGT_BITMAP_BGRA)
    {
	if (X11Adapter_glitches() & XG_RENDER_SRC_OFFSET
		&& memcmp(&pos, &self->pos, sizeof pos))
	{
	    xcb_render_transform_t shift = {
		1 << 16, 0, -pos.x << 16,
		0, 1 << 16, -pos.y << 16,
		0, 0, 1 << 16
	    };
	    CHECK(xcb_render_set_picture_transform(c, self->tpic, shift),
		    "TextRenderer: Cannot shift temporary picture for 0x%x",
		    (unsigned)self->pixmap);
	    self->pos = pos;
	}
	srcpic = self->tpic;
    }
    else if (color != self->color)
    {
	xcb_rectangle_t rect = { 0, 0, 1, 1 };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    self->pen, Color_xcb(color), 1, &rect),
		"TextRenderer: Cannot colorize pen for 0x%x",
		(unsigned)self->pixmap);
	self->color = color;
    }
    CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER, srcpic,
		picture, 0, Font_glyphtype(self->font) == FGT_BITMAP_BGRA ?
		Font_maskGlyphset(self->font) : Font_glyphset(self->font),
		0, ody, self->hblen * sizeof *self->glyphs,
		(const uint8_t *)self->glyphs),
	    "TextRenderer: Cannot render glyphs for 0x%x",
	    (unsigned)self->pixmap);
    self->glyphs[0].dx = odx;
    self->glyphs[0].dy = ody;
    return 0;
}

int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos)
{
    return TextRenderer_renderWithSelection(self, picture, color, pos,
	    (Selection){0, 0}, 0);
}

void TextRenderer_destroy(TextRenderer *self)
{
    if (!self) return;
    free(self->glyphs);
    hb_buffer_destroy(self->hbbuffer);
    hb_font_destroy(self->hbfont);
    xcb_connection_t *c = X11Adapter_connection();
    if (self->tpic) xcb_render_free_picture(c, self->tpic);
    xcb_render_free_picture(c, self->pen);
    if (self->tmp) xcb_free_pixmap(c, self->tmp);
    xcb_free_pixmap(c, self->pixmap);
    free(self);
}

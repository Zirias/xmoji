#include "textrenderer.h"

#include "font.h"
#include "pen.h"
#include "unistr.h"
#include "widget.h"
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
    Widget *owner;
    Font *font;
    hb_font_t *hbfont;
    hb_buffer_t *hbbuffer;
    hb_glyph_info_t *hbglyphs;
    hb_glyph_position_t *hbpos;
    GlyphRenderInfo *glyphs;
    Pen *pen;
    xcb_render_picture_t tpic;
    Color color;
    Color tcolor;
    Color scolor;
    Size size;
    Pos pos;
    Selection selection;
    unsigned hblen;
    int noligatures;
    int uploaded;
};

static void clearRenderer(TextRenderer *self)
{
    free(self->glyphs);
    self->glyphs = 0;
    self->uploaded = 0;
    hb_buffer_destroy(self->hbbuffer);
    self->hbbuffer = 0;
    hb_font_destroy(self->hbfont);
    self->hbfont = 0;
    if (self->tpic)
    {
	xcb_render_free_picture(X11Adapter_connection(), self->tpic);
    }
    self->tpic = 0;
    Font_destroy(self->font);
    self->font = 0;
}

TextRenderer *TextRenderer_create(Widget *owner)
{
    TextRenderer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    self->owner = owner;
    self->pen = Pen_create();
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

void TextRenderer_setFont(TextRenderer *self, Font *font)
{
    clearRenderer(self);
    self->font = Font_ref(font);
    self->hbfont = hb_ft_font_create_referenced(Font_face(font));
    if (Font_glyphtype(font) == FGT_BITMAP_BGRA)
    {
	Pen_configure(self->pen, PICTFORMAT_RGB, 0xffffffff);
    }
}

Font *TextRenderer_font(TextRenderer *self)
{
    return self->font;
}

static void createTmpPicture(TextRenderer *self, xcb_connection_t *c)
{
    xcb_render_picture_t ownerpic = Widget_picture(self->owner);
    xcb_pixmap_t tmp = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 24, tmp, X11Adapter_screen()->root,
		self->size.width, self->size.height),
	    "TextRenderer: Cannot create temporary pixmap for 0x%x",
	    (unsigned)ownerpic);
    self->tpic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, self->tpic, tmp,
		X11Adapter_rgbformat(), 0, 0),
	    "TextRenderer: Cannot create temporary picture for 0x%x",
	    (unsigned)ownerpic);
    xcb_free_pixmap(c, tmp);
    if (Font_glyphtype(self->font) == FGT_BITMAP_BGRA)
    {
	CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_IN,
		    Pen_picture(self->pen, ownerpic), self->tpic, 0,
		    Font_glyphset(self->font), 0, 0,
		    self->hblen * sizeof *self->glyphs,
		    (const uint8_t *)self->glyphs),
		"TextRenderer: Cannot render glyphs for 0x%x",
		(unsigned)ownerpic);
    }
    if (X11Adapter_glitches() & XG_RENDER_SRC_OFFSET)
    {
	self->pos = (Pos){0, 0};
    }
}

int TextRenderer_setText(TextRenderer *self, const UniStr *text)
{
    if (!self->font) return -1;
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
    if (!self->size.width) self->size.width = 1;
    self->size.height = (height + 0x3fU) >> 6;
    if (!self->size.height) self->size.height = 1;
    xcb_connection_t *c = X11Adapter_connection();
    if (self->tpic)
    {
	xcb_render_free_picture(c, self->tpic);
	self->tpic = 0;
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
    self->uploaded = 0;
    return 0;
}

unsigned TextRenderer_nglyphs(const TextRenderer *self)
{
    return self->hblen;
}

uint32_t TextRenderer_glyphIdAt(const TextRenderer *self, unsigned index)
{
    if (index >= self->hblen) return 0;
    return self->hbglyphs[index].codepoint;
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

unsigned TextRenderer_charIndex(const TextRenderer *self, unsigned pixelpos)
{
    uint32_t pos = pixelpos << 6;
    uint32_t offset = 0;
    for (unsigned i = 0; i < self->hblen - 1; ++i)
    {
	uint32_t afterpos = offset + self->hbpos[i].x_advance;
	if (afterpos > pos)
	{
	    if (afterpos - pos < pos - offset) ++i;
	    return self->hbglyphs[i].cluster;
	}
	offset = afterpos;
    }
    return (unsigned)-1;
}

int TextRenderer_renderWithSelection(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos,
	Selection selection, Color selectionColor)
{
    if (!self->hbbuffer) return -1;
    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_picture_t ownerpic = Widget_picture(self->owner);
    if (!self->uploaded)
    {
	Font_uploadGlyphs(self->font, ownerpic, self->hblen, self->glyphs);
	self->uploaded = 1;
    }
    xcb_render_picture_t srcpic;
    if (selection.len || Font_glyphtype(self->font) == FGT_BITMAP_BGRA)
    {
	if (!self->tpic) createTmpPicture(self, c);
	if (selection.len &&
		(memcmp(&selection, &self->selection, sizeof selection)
		    || color != self->tcolor
		    || selectionColor != self->scolor))
	{
	    xcb_rectangle_t rect = { 0, 0,
		self->size.width, self->size.height };
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			self->tpic, Color_xcb(color), 1, &rect),
		    "TextRenderer: Cannot colorize foreground for 0x%x",
		    (unsigned)ownerpic);
	    rect.x = selection.start;
	    rect.width = selection.len;
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			self->tpic, Color_xcb(selectionColor), 1, &rect),
		    "TextRenderer: Cannot colorize selection for 0x%x",
		    (unsigned)ownerpic);
	    self->scolor = selectionColor;
	    self->tcolor = color;
	    self->selection = selection;
	}
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
		    (unsigned)ownerpic);
	    self->pos = pos;
	}
	srcpic = self->tpic;
    }
    else
    {
	if (color != self->color)
	{
	    Pen_configure(self->pen, PICTFORMAT_RGB, color);
	    self->color = color;
	}
	srcpic = Pen_picture(self->pen, ownerpic);
    }
    uint16_t odx = self->glyphs[0].dx;
    uint16_t ody = self->glyphs[0].dy;
    self->glyphs[0].dx += pos.x;
    self->glyphs[0].dy += pos.y;
    CHECK(xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER, srcpic,
		picture, 0, Font_glyphtype(self->font) == FGT_BITMAP_BGRA ?
		Font_maskGlyphset(self->font) : Font_glyphset(self->font),
		0, ody, self->hblen * sizeof *self->glyphs,
		(const uint8_t *)self->glyphs),
	    "TextRenderer: Cannot render glyphs for 0x%x",
	    (unsigned)ownerpic);
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
    clearRenderer(self);
    Pen_destroy(self->pen);
    free(self);
}

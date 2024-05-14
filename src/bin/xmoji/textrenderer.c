#include "textrenderer.h"

#include "font.h"

#include <hb.h>
#include <hb-ft.h>
#include <poser/core.h>
#include <string.h>

struct TextRenderer
{
    hb_font_t *hbfont;
    hb_buffer_t *hbbuffer;
    PSC_ThreadJob *shapejob;
    void *shapejobctx;
    TR_size_cb sizecb;
    int shaped;
    uint32_t width;
    uint32_t height;
};

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

TextRenderer *TextRenderer_fromUtf8(const Font *font, const char *utf8)
{
    TextRenderer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
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

void TextRenderer_destroy(TextRenderer *self)
{
    if (!self) return;
    hb_buffer_destroy(self->hbbuffer);
    hb_font_destroy(self->hbfont);
    free(self);
}

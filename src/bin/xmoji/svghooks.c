#include "svghooks.h"

#include "nanosvg.h"

#include <math.h>
#include <ft2build.h>
#include FT_OTSVG_H
#include <poser/core.h>

typedef struct SvgGlyph
{
    NSVGimage *svg;
    uint32_t scale;
    float xoff;
    float yoff;
} SvgGlyph;

typedef struct SvgRenderer
{
    NSVGrasterizer *rast;
    SvgGlyph *glyph;
} SvgRenderer;

static void destroyGlyph(SvgGlyph *self)
{
    if (!self) return;
    nsvgDelete(self->svg);
    free(self);
}

static void destroyRenderer(SvgRenderer *self)
{
    if (!self) return;
    destroyGlyph(self->glyph);
    nsvgDeleteRasterizer(self->rast);
    free(self);
}

static FT_Error init_svg(FT_Pointer *data_pointer)
{
    SvgRenderer *renderer = PSC_malloc(sizeof *renderer);
    renderer->rast = nsvgCreateRasterizer();
    renderer->glyph = 0;
    *data_pointer = renderer;
    return FT_Err_Ok;
}

static void free_svg(FT_Pointer *data_pointer)
{
    destroyRenderer(*data_pointer);
    *data_pointer = 0;
}

static FT_Error render_svg(FT_GlyphSlot slot, FT_Pointer *data_pointer)
{
    SvgRenderer *renderer = *data_pointer;
    SvgGlyph *glyph = renderer->glyph;
    if (!glyph) return FT_Err_Invalid_SVG_Document;
    float scale = (float)glyph->scale / (float)(1<<22);
    nsvgRasterize(renderer->rast, glyph->svg, glyph->xoff * scale,
	    glyph->yoff * scale, scale, slot->bitmap.buffer,
	    slot->bitmap.width, slot->bitmap.rows, slot->bitmap.pitch);
    for (unsigned y = 0; y < slot->bitmap.rows; ++y)
    {
	uint8_t *row = slot->bitmap.buffer + y * slot->bitmap.pitch;
	for (unsigned x = 0; x < slot->bitmap.width; ++x)
	{
	    uint8_t alpha = row[4*x+3];
	    if (alpha)
	    {
		uint8_t red = alpha * row[4*x] / 0xffU;
		uint8_t blue = alpha * row[4*x+2] / 0xffU;
		row[4*x] = blue;
		row[4*x+1] = alpha * row[4*x+1] / 0xffU;
		row[4*x+2] = red;
	    }
	    else
	    {
		row[4*x] = 0;
		row[4*x+1] = 0;
		row[4*x+2] = 0;
	    }
	}
    }
    slot->format = FT_GLYPH_FORMAT_BITMAP;
    return FT_Err_Ok;
}

static FT_Error preset_slot(FT_GlyphSlot slot,
	FT_Bool cache, FT_Pointer *data_pointer)
{
    FT_SVG_Document doc = (FT_SVG_Document)slot->other;
    if (doc->start_glyph_id != doc->end_glyph_id)
    {
	return FT_Err_Unimplemented_Feature;
    }

    char *svgstr = PSC_malloc(doc->svg_document_length + 1);
    memcpy(svgstr, doc->svg_document, doc->svg_document_length);
    svgstr[doc->svg_document_length] = 0;
    NSVGimage *svg = nsvgParse(svgstr, "px", 0.);
    free(svgstr);
    if (!svg) return FT_Err_Invalid_SVG_Document;

    SvgGlyph glyph;
    glyph.svg = svg;

    float xmin = HUGE_VALF;
    float ymin = HUGE_VALF;
    float xmax = -HUGE_VALF;
    float ymax = -HUGE_VALF;
    for (const struct NSVGshape *shape = svg->shapes; shape;
	    shape = shape->next)
    {
	if (shape->bounds[0] < xmin) xmin = shape->bounds[0];
	if (shape->bounds[1] < ymin) ymin = shape->bounds[1];
	if (shape->bounds[2] > xmax) xmax = shape->bounds[2];
	if (shape->bounds[3] > ymax) ymax = shape->bounds[3];
    }
    uint16_t svgwidth = ceilf(xmax - xmin);
    uint16_t svgheight = ceilf(ymax - ymin);
    if (!svgwidth || !svgheight)
    {
	svgwidth = doc->units_per_EM;
	svgheight = doc->units_per_EM;
    }
    glyph.scale = doc->metrics.x_scale < doc->metrics.y_scale
	    ? doc->metrics.x_scale : doc->metrics.y_scale;
    glyph.xoff = -xmin;
    glyph.yoff = -ymin;

    uint32_t width = FT_MulFix(svgwidth, glyph.scale);
    uint32_t height = FT_MulFix(svgheight, glyph.scale);

    slot->bitmap.rows = (height + 0x3fU) >> 6;
    slot->bitmap.width = (width + 0x3fU) >> 6;
    slot->bitmap.pitch = slot->bitmap.width << 2;
    slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    slot->bitmap.num_grays = 256;
    slot->bitmap_left = slot->metrics.horiAdvance > width ?
	(slot->metrics.horiAdvance - width) >> 7 : 0;
    slot->bitmap_top = (doc->metrics.height +
	    doc->metrics.descender + 0x3f) >> 6;
    if (slot->bitmap_top > (int)slot->bitmap.rows)
    {
	slot->bitmap_top = slot->bitmap.rows;
    }
    slot->metrics.width = width;
    slot->metrics.height = height;
    slot->metrics.horiBearingX = slot->bitmap_left * 64;
    slot->metrics.horiBearingY = -slot->bitmap_top * 64;
    slot->metrics.vertBearingX = -slot->metrics.horiAdvance * 32;
    slot->metrics.vertBearingY =
	(slot->metrics.vertAdvance - slot->metrics.height) *32;

    if (cache)
    {
	SvgRenderer *renderer = *data_pointer;
	destroyGlyph(renderer->glyph);
	renderer->glyph = PSC_malloc(sizeof *renderer->glyph);
	memcpy(renderer->glyph, &glyph, sizeof *renderer->glyph);
    }
    return FT_Err_Ok;
}

static const SVG_RendererHooks svghooks = {
    .init_svg = init_svg,
    .free_svg = free_svg,
    .render_svg = render_svg,
    .preset_slot = preset_slot
};

const void *SvgHooks_get(void)
{
    return &svghooks;
}


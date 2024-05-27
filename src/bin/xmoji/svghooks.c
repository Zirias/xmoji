#include "svghooks.h"

#include "nanosvg.h"

#include <math.h>
#include <ft2build.h>
#include FT_OTSVG_H
#include <poser/core.h>

typedef struct SvgRenderer
{
    NSVGimage *svg;
    uint32_t scale;
    float xoff;
    float yoff;
} SvgRenderer;

static void finalize_slot(void *obj)
{
    FT_GlyphSlot slot = obj;
    SvgRenderer *renderer = slot->generic.data;
    nsvgDelete(renderer->svg);
    free(renderer);
    slot->generic.data = 0;
    slot->generic.finalizer = 0;
}

static FT_Error init_svg(FT_Pointer *data_pointer)
{
    *data_pointer = nsvgCreateRasterizer();
    return FT_Err_Ok;
}

static void free_svg(FT_Pointer *data_pointer)
{
    nsvgDeleteRasterizer(*data_pointer);
    *data_pointer = 0;
}

static FT_Error render_svg(FT_GlyphSlot slot, FT_Pointer *data_pointer)
{
    SvgRenderer *renderer = slot->generic.data;
    if (!renderer) return FT_Err_Invalid_SVG_Document;
    NSVGrasterizer *rast = *data_pointer;
    float scale = (float)renderer->scale / (float)(1<<22);
    nsvgRasterize(rast, renderer->svg, renderer->xoff * scale,
	    renderer->yoff * scale, scale, slot->bitmap.buffer,
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
	FT_Bool cache, FT_Pointer *state)
{
    (void)cache;
    (void)state;

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

    SvgRenderer *renderer = PSC_malloc(sizeof *renderer);
    renderer->svg = svg;
    slot->generic.data = renderer;
    slot->generic.finalizer = finalize_slot;

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
    renderer->scale = doc->metrics.x_scale < doc->metrics.y_scale
	    ? doc->metrics.x_scale : doc->metrics.y_scale;
    renderer->xoff = -xmin;
    renderer->yoff = -ymin;

    uint32_t width = FT_MulFix(svgwidth, renderer->scale);
    uint32_t height = FT_MulFix(svgheight, renderer->scale);

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


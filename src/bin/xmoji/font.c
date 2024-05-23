#include "font.h"
#include "x11adapter.h"

#include <fontconfig/fontconfig.h>
#include FT_OUTLINE_H
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

FT_Library ftlib;
int refcnt;
FcPattern *defaultpat;
double defaultpixelsize;

struct Font
{
    FT_Face face;
    double pixelsize;
};

typedef struct OutlineFont
{
    Font base;
    int haserror;
    uint16_t uploading;
    uint8_t glyphidbits;
    uint8_t subpixelbits;
    uint32_t glyphidmask;
    uint32_t subpixelmask;
    xcb_render_glyphset_t glyphset;
    uint32_t uploaded[];
} OutlineFont;

int Font_init(void)
{
    if (refcnt++) return 0;

    if (FcInit() != FcTrue)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not initialize fontconfig");
	goto error;
    }
    defaultpat = FcNameParse((FcChar8 *)"sans");
    FcConfigSubstitute(0, defaultpat, FcMatchPattern);
    FcDefaultSubstitute(defaultpat);
    FcPatternGetDouble(defaultpat, FC_PIXEL_SIZE, 0, &defaultpixelsize);

    if (FT_Init_FreeType(&ftlib) != 0)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not initialize freetype");
	goto error;
    }

    return 0;

error:
    Font_done();
    return -1;
}

void Font_done(void)
{
    if (--refcnt) return;
    FT_Done_FreeType(ftlib);
    FcPatternDestroy(defaultpat);
    FcFini();
}

static void requestError(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    OutlineFont *of = receiver;
    PSC_Log_fmt(PSC_L_ERROR, "Font glyphset 0x%x failed",
	    (unsigned)of->glyphset);
    of->haserror = 1;
}

Font *Font_create(uint8_t subpixelbits, const char *pattern)
{
    PSC_List *patterns = 0;
    PSC_ListIterator *pi = 0;
    if (pattern)
    {
	patterns = PSC_List_fromString(pattern, ",");
	pi = PSC_List_iterator(patterns);
    }

    int ismatch = 0;
    FcPattern *fcfont;
    FT_Face face;
    double pixelsize = defaultpixelsize;
    double fixedpixelsize = 0;
    int defstep = 1;
    while (!ismatch && ((pi && PSC_ListIterator_moveNext(pi)) || defstep--))
    {
	const char *patstr = defstep ? PSC_ListIterator_current(pi) : "";
	FcPattern *fcpat;
	if (*patstr)
	{
	    PSC_Log_fmt(PSC_L_DEBUG, "Looking for font: %s", patstr);
	    fcpat = FcNameParse((FcChar8 *)patstr);
	    double reqsize = 0;
	    double reqpxsize = 0;
	    FcPatternGetDouble(fcpat, FC_SIZE, 0, &reqsize);
	    FcPatternGetDouble(fcpat, FC_PIXEL_SIZE, 0, &reqpxsize);
	    if (!reqsize && !reqpxsize)
	    {
		FcPatternAddDouble(fcpat, FC_PIXEL_SIZE, pixelsize);
	    }
	    FcConfigSubstitute(0, fcpat, FcMatchPattern);
	    FcDefaultSubstitute(fcpat);
	    FcPatternGetDouble(fcpat, FC_PIXEL_SIZE, 0, &pixelsize);
	}
	else
	{
	    PSC_Log_msg(PSC_L_DEBUG, "Looking for default font");
	    fcpat = defaultpat;
	}
	FcResult result;
	fcfont = FcFontMatch(0, fcpat, &result);
	ismatch = (result == FcResultMatch);
	FcChar8 *foundfamily = 0;
	if (ismatch) FcPatternGetString(fcfont, FC_FAMILY, 0, &foundfamily);
	if (ismatch && *patstr)
	{
	    FcChar8 *reqfamily = 0;
	    FcPatternGetString(fcpat, FC_FAMILY, 0, &reqfamily);
	    if (reqfamily && (!foundfamily ||
			strcmp((const char *)reqfamily,
			    (const char *)foundfamily)))
	    {
		ismatch = 0;
	    }
	}
	fixedpixelsize = 0;
	if (fcpat != defaultpat) FcPatternDestroy(fcpat);
	fcpat = 0;
	FcChar8 *fontfile = 0;
	if (ismatch)
	{
	    FcPatternGetString(fcfont, FC_FILE, 0, &fontfile);
	    if (!fontfile)
	    {
		PSC_Log_msg(PSC_L_WARNING, "Found font without a file");
		ismatch = 0;
	    }
	}
	int fontindex = 0;
	if (ismatch)
	{
	    FcPatternGetInteger(fcfont, FC_INDEX, 0, &fontindex);
	    if (FT_New_Face(ftlib,
			(const char *)fontfile, fontindex, &face) == 0)
	    {
		if (!(face->face_flags & FT_FACE_FLAG_SCALABLE))
		{
		    unsigned targetpx = (unsigned)(64.0 * pixelsize);
		    int bestdiff = INT_MIN;
		    int bestidx = 0;
		    for (int i = 0; i < face->num_fixed_sizes; ++i)
		    {
			int diff = face->available_sizes[i].y_ppem - targetpx;
			if (!diff)
			{
			    bestdiff = diff;
			    bestidx = i;
			    break;
			}
			if (diff < 0)
			{
			    if (bestdiff > 0) continue;
			    if (diff > bestdiff)
			    {
				bestdiff = diff;
				bestidx = i;
			    }
			}
			else
			{
			    if (bestdiff < 0 || diff < bestdiff)
			    {
				bestdiff = diff;
				bestidx = i;
			    }
			}
		    }
		    if (FT_Select_Size(face, bestidx) == 0)
		    {
			fixedpixelsize =
			    (double)face->available_sizes[bestidx].y_ppem
			    / 64.0;
		    }
		    else
		    {
			PSC_Log_msg(PSC_L_WARNING,
				"Cannot select best matching font size");
			ismatch = 0;
		    }
		}
		else if (FT_Set_Char_Size(face, 0,
			    (unsigned)(64.0 * pixelsize), 0, 0) != 0)
		{
		    PSC_Log_msg(PSC_L_WARNING, "Cannot set desired font size");
		    ismatch = 0;
		}
	    }
	    else
	    {
		PSC_Log_fmt(PSC_L_WARNING, "Cannot open font file %s",
			(const char *)fontfile);
		ismatch = 0;
	    }
	}
	if (ismatch)
	{
	    PSC_Log_fmt(PSC_L_INFO, "Font `%s:pixelsize=%.2f' found in `%s'",
		    (const char *)foundfamily,
		    fixedpixelsize ? fixedpixelsize : pixelsize,
		    (const char *)fontfile);
	}
	FcPatternDestroy(fcfont);
	fcfont = 0;
    }
    PSC_ListIterator_destroy(pi);
    PSC_List_destroy(patterns);

    if (!ismatch)
    {
	PSC_Log_fmt(PSC_L_WARNING, "No matching font found for `%s'", pattern);
    }

    Font *self;
    if (fixedpixelsize)
    {
	self = PSC_malloc(sizeof *self);
	self->pixelsize = fixedpixelsize;
    }
    else
    {
	uint8_t glyphidbits = 1;
	uint32_t glyphidmask = 1;
	while ((face->num_glyphs & glyphidmask) != face->num_glyphs)
	{
	    ++glyphidbits;
	    glyphidmask <<= 1;
	    glyphidmask |= 1;
	}
	if (subpixelbits > 6) subpixelbits = 6;
	size_t ofsz;
	OutlineFont *of = PSC_malloc(((ofsz = sizeof *of +
			(1U << (glyphidbits + subpixelbits - 5))
			* sizeof *of->uploaded)));
	memset(of, 0, ofsz);
	xcb_connection_t *c = X11Adapter_connection();
	of->glyphset = xcb_generate_id(c);
	PSC_Event_register(X11Adapter_requestError(), of,
		requestError, of->glyphset);
	CHECK(xcb_render_create_glyph_set(c,
		    of->glyphset, X11Adapter_alphaformat()),
		"Font: Cannot create glyphset 0x%x", (unsigned)of->glyphset);
	of->glyphidbits = glyphidbits;
	of->subpixelbits = subpixelbits;
	of->glyphidmask = glyphidmask;
	of->subpixelmask = ((1U << subpixelbits) - 1) << glyphidbits;
	self = (Font *)of;
	self->pixelsize = pixelsize;
    }
    self->face = face;
    return self;
}

FT_Face Font_face(const Font *self)
{
    return self->face;
}

double Font_pixelsize(const Font *self)
{
    return self->pixelsize;
}

uint16_t Font_linespace(const Font *self)
{
    return (self->face->size->metrics.height + 0x20) >> 6;
}

uint8_t Font_glyphidbits(const Font *self)
{
    if (!(self->face->face_flags & FT_FACE_FLAG_SCALABLE)) return 16;
    OutlineFont *of = (OutlineFont *)self;
    return of->glyphidbits;
}

uint8_t Font_subpixelbits(const Font *self)
{
    if (!(self->face->face_flags & FT_FACE_FLAG_SCALABLE)) return 0;
    OutlineFont *of = (OutlineFont *)self;
    return of->subpixelbits;
}

int Font_uploadGlyphs(Font *self, unsigned len, GlyphRenderInfo *glyphinfo)
{
    if (!(self->face->face_flags & FT_FACE_FLAG_SCALABLE)) return -1;
    OutlineFont *of = (OutlineFont *)self;
    int rc = -1;
    unsigned toupload = 0;
    uint32_t *glyphids = PSC_malloc(len * sizeof *glyphids);
    xcb_render_glyphinfo_t *glyphs = 0;
    unsigned firstglyph = 0;
    uint8_t *bitmapdata = 0;
    size_t bitmapdatasz = 0;
    uint32_t maxglyphid = of->glyphidmask | of->subpixelmask;
    for (unsigned i = 0; i < len; ++i)
    {
	if (glyphinfo[i].glyphid > maxglyphid) goto done;
	uint32_t word = glyphinfo[i].glyphid >> 5;
	uint32_t bit = 1U << (glyphinfo[i].glyphid & 0x1fU);
	if (of->uploaded[word] & bit) continue;
	glyphids[toupload++] = glyphinfo[i].glyphid;
    }
    if (!toupload)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Font: Nothing to upload for glyphset 0x%x",
		(unsigned)of->glyphset);
	rc = 0;
	goto done;
    }
    glyphs = PSC_malloc(toupload * sizeof *glyphs);
    memset(glyphs, 0, toupload * sizeof *glyphs);
    size_t bitmapdatapos = 0;
    xcb_connection_t *c = X11Adapter_connection();
    for (unsigned i = 0; i < toupload; ++i)
    {
	if (FT_Load_Glyph(self->face, glyphids[i] & of->glyphidmask,
		    FT_LOAD_NO_BITMAP) != 0) goto done;
	FT_GlyphSlot slot = self->face->glyph;
	uint32_t xshift = glyphids[i] >> of->glyphidbits
	    << (6 - of->subpixelbits);
	if (xshift)
	{
	    FT_Outline_Translate(&slot->outline, xshift, 0);
	}
	FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	glyphs[i].x = -slot->bitmap_left;
	glyphs[i].y = slot->bitmap_top;
	glyphs[i].width = slot->bitmap.width;
	glyphs[i].height = slot->bitmap.rows;
	unsigned stride = (slot->bitmap.width+3)&~3;
	size_t bitmapsz = stride * slot->bitmap.rows;
	if (sizeof (xcb_render_add_glyphs_request_t)
		+ (i - firstglyph) * (sizeof *glyphids + sizeof *glyphs)
		+ bitmapdatapos
		+ bitmapsz
		> X11Adapter_maxRequestSize())
	{
	    CHECK(xcb_render_add_glyphs(c, of->glyphset, i - firstglyph,
			glyphids + firstglyph, glyphs + firstglyph,
			bitmapdatapos, bitmapdata),
		    "Cannot upload to glyphset 0x%x", (unsigned)of->glyphset);
	    for (unsigned j = firstglyph; j < i; ++j)
	    {
		uint32_t word = glyphids[j] >> 5;
		uint32_t bit = 1U << (glyphids[j] & 0x1fU);
		of->uploaded[word] |= bit;
	    }
	    bitmapdatapos = 0;
	    firstglyph = i;
	}
	if (bitmapdatapos + bitmapsz > bitmapdatasz)
	{
	    bitmapdata = PSC_realloc(bitmapdata, bitmapdatapos + bitmapsz);
	    bitmapdatasz = bitmapdatapos + bitmapsz;
	}
	for (unsigned y = 0; y < slot->bitmap.rows; ++y)
	{
	    memcpy(bitmapdata + bitmapdatapos + y * stride,
		    slot->bitmap.buffer + y * slot->bitmap.width,
		    slot->bitmap.width);
	}
	bitmapdatapos += bitmapsz;
    }
    CHECK(xcb_render_add_glyphs(c, of->glyphset, toupload - firstglyph,
		glyphids + firstglyph, glyphs + firstglyph,
		bitmapdatapos, bitmapdata),
	    "Cannot upload to glyphset 0x%x", (unsigned)of->glyphset);
    for (unsigned i = firstglyph; i < toupload; ++i)
    {
	uint32_t word = glyphids[i] >> 5;
	uint32_t bit = 1U << (glyphids[i] & 0x1fU);
	of->uploaded[word] |= bit;
    }
    rc = 0;
done:
    free(bitmapdata);
    free(glyphs);
    free(glyphids);
    return rc;
}

xcb_render_glyphset_t Font_glyphset(const Font *self)
{
    if (!(self->face->face_flags & FT_FACE_FLAG_SCALABLE)) return 0;
    const OutlineFont *of = (const OutlineFont *)self;
    return of->glyphset;
}

void Font_destroy(Font *self)
{
    if (!self) return;
    if (self->face->face_flags & FT_FACE_FLAG_SCALABLE)
    {
	OutlineFont *of = (OutlineFont *)self;
	PSC_Event_unregister(X11Adapter_requestError(), of,
		requestError, of->glyphset);
    }
    FT_Done_Face(self->face);
    free(self);
}

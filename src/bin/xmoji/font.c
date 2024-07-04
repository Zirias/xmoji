#include "font.h"

#include "svghooks.h"
#include "x11adapter.h"
#include "xrdb.h"

#include <fontconfig/fontconfig.h>
#include FT_MODULE_H
#include FT_OUTLINE_H
#include <math.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static FT_Library ftlib;
static int refcnt;
static FcPattern *defaultpat;
static PSC_HashTable *byPattern;
static PSC_HashTable *byId;
static double defaultpixelsize;

static FontOptions defaultOptions;

struct Font
{
    char *id;
    FcPattern *pattern;
    FT_Face face;
    FontGlyphType glyphtype;
    double pixelsize;
    double fixedpixelsize;
    int refcnt;
    uint16_t uploading;
    uint8_t glyphidbits;
    uint8_t subpixelbits;
    uint32_t glyphidmask;
    uint32_t subpixelmask;
    xcb_render_glyphset_t glyphset;
    xcb_render_glyphset_t maskglyphset;
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t baseline;
    uint32_t uploaded[];
};

static int Font_init(void);
static void Font_done(void);

static int Font_init(void)
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

    if (FT_Property_Set(ftlib, "ot-svg", "svg-hooks", SvgHooks_get()) != 0)
    {
	PSC_Log_msg(PSC_L_WARNING, "Could not add SVG rendering hooks");
    }

    byPattern = PSC_HashTable_create(5);
    byId = PSC_HashTable_create(5);

    XRdb *rdb = X11Adapter_resources();
    if (rdb)
    {
	XRdb_register(rdb, "FontOptions", "defaultFontOptions");
	defaultOptions.maxUnscaledDeviation = XRdb_float(rdb,
		XRdbKey("defaultFontOptions", "maxUnscaledDeviation"),
		XRQF_OVERRIDES, 5., .1, 100.);
	defaultOptions.pixelFractionBits = XRdb_int(rdb,
		XRdbKey("defaultFontOptions", "pixelFractionBits"),
		XRQF_OVERRIDES, 3, 0, 6);
    }
    else
    {
	defaultOptions.maxUnscaledDeviation = 5.f;
	defaultOptions.pixelFractionBits = 3;
    }
    return 0;

error:
    Font_done();
    return -1;
}

static void Font_done(void)
{
    if (--refcnt) return;
    PSC_HashTable_destroy(byId);
    byId = 0;
    PSC_HashTable_destroy(byPattern);
    byPattern = 0;
    FT_Done_FreeType(ftlib);
    FcPatternDestroy(defaultpat);
    FcFini();
}

static char *createFontId(const char *file, int index, double pixelsize)
{
    int fi = index;
    int fic = 0;
    while (fi) { ++fic; fi /= 10; }
    if (!fic) ++fic;
    size_t len = strlen(file) + fic + 9;
    char *id = PSC_malloc(len);
    snprintf(id, len, "%s:%d:%06.2f", file, index, pixelsize);
    return id;
}

static Font *createFromFile(const char *file, int index, char *id,
	FcPattern *pattern, const FontOptions *options, double pixelsize)
{
    double fixedpixelsize = 0;
    FT_Face face = 0;
    if (FT_New_Face(ftlib, file, index, &face) == 0)
    {
	if (!(face->face_flags & FT_FACE_FLAG_SCALABLE))
	{
	    /* Check available fixed sizes, pick the best match.
	     * Prefer the smallest deviation within the configured
	     * range allowed to be used unscaled. Otherwise prefer
	     * the largest available size.
	     */
	    double maxunscaleddeviation =
		(options && options->maxUnscaledDeviation >= .1f
		 ? options->maxUnscaledDeviation
		 : defaultOptions.maxUnscaledDeviation) / 100.;
	    double bestdeviation = HUGE_VAL;
	    int bestidx = -1;
	    for (int i = 0; i < face->num_fixed_sizes; ++i)
	    {
		double fpx =
		    (double)face->available_sizes[i].y_ppem / 64.;
		double dev = (fpx > pixelsize ? fpx / pixelsize :
			pixelsize / fpx) - 1.;
		if (bestidx < 0 || dev < bestdeviation ||
			(fpx > fixedpixelsize &&
			 bestdeviation > maxunscaleddeviation))
		{
		    fixedpixelsize = fpx;
		    bestdeviation = dev;
		    bestidx = i;
		}
	    }
	    if (FT_Select_Size(face, bestidx) != 0)
	    {
		PSC_Log_msg(PSC_L_WARNING,
			"Cannot select best matching font size");
		FT_Done_Face(face);
		return 0;
	    }
	    if (bestdeviation <= maxunscaleddeviation)
	    {
		pixelsize = fixedpixelsize;
	    }
	}
	else if (FT_Set_Char_Size(face, 0,
		    (unsigned)(64.0 * pixelsize), 0, 0) != 0)
	{
	    PSC_Log_msg(PSC_L_WARNING, "Cannot set desired font size");
	    FT_Done_Face(face);
	    return 0;
	}
    }
    else
    {
	PSC_Log_fmt(PSC_L_WARNING, "Cannot open font file %s", file);
	return 0;
    }

    char *family = 0;
    FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8 **)&family);
    if (fixedpixelsize && fixedpixelsize != pixelsize)
    {
	PSC_Log_fmt(PSC_L_INFO, "Font `%s:pixelsize=%.2f' "
		"(scaled from pixelsize=%.2f) found in `%s'",
		family, pixelsize, fixedpixelsize, file);
    }
    else
    {
	PSC_Log_fmt(PSC_L_INFO, "Font `%s:pixelsize=%.2f' "
		"found in `%s'", family, pixelsize, file);
    }

    uint8_t subpixelbits;
    if (fixedpixelsize) subpixelbits = 0;
    else
    {
	subpixelbits = options && options->pixelFractionBits != (uint8_t)-1
	    ? options->pixelFractionBits : defaultOptions.pixelFractionBits;
	if (subpixelbits > 6) subpixelbits = 6;
    }
    uint8_t glyphidbits = 1;
    uint32_t glyphidmask = 1;
    while ((face->num_glyphs & glyphidmask) != face->num_glyphs)
    {
	++glyphidbits;
	glyphidmask <<= 1;
	glyphidmask |= 1;
    }
    size_t fsz;
    Font *self = PSC_malloc(((fsz = sizeof *self +
		    (1U << (glyphidbits + subpixelbits - 5))
		    * sizeof *self->uploaded)));
    memset(self, 0, fsz);

    if (id) PSC_Log_fmt(PSC_L_DEBUG, "Font id: %s", id);
    self->id = id;
    self->pattern = pattern;
    self->face = face;
    double scale = 0;
    if (fixedpixelsize)
    {
	self->glyphtype = face->face_flags & FT_FACE_FLAG_COLOR ?
	    FGT_BITMAP_BGRA : FGT_BITMAP_GRAY;
	scale = pixelsize / fixedpixelsize;
	face->size->metrics.x_scale = face->size->metrics.x_scale * scale + 1.;
	face->size->metrics.y_scale = face->size->metrics.y_scale * scale + 1.;
    }
    else self->glyphtype = face->face_flags & FT_FACE_FLAG_COLOR ?
	FGT_BITMAP_BGRA : FGT_OUTLINE;
    self->pixelsize = pixelsize;
    self->fixedpixelsize = fixedpixelsize;
    self->refcnt = 1;
    self->glyphidbits = glyphidbits;
    self->subpixelbits = subpixelbits;
    self->glyphidmask = glyphidmask;
    self->subpixelmask = ((1U << subpixelbits) - 1) << glyphidbits;
    uint32_t claimedheight;
    if (scale) claimedheight = scale * (face->size->metrics.ascender
	    - face->size->metrics.descender) + 1.;
    else claimedheight = face->size->metrics.ascender
	- face->size->metrics.descender;
    self->maxWidth = FT_MulFix(face->bbox.xMax, face->size->metrics.x_scale)
	- FT_MulFix(face->bbox.xMin, face->size->metrics.x_scale);
    self->maxHeight = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale)
	- FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale);
    if (!self->maxHeight ||
	    (claimedheight && self->maxHeight >= claimedheight << 1))
    {
	self->maxHeight = claimedheight;
	if (scale) self->baseline = scale * face->size->metrics.ascender + 1.;
	else self->baseline = face->size->metrics.ascender;
    }
    else self->baseline = FT_MulFix(face->bbox.yMax,
	    face->size->metrics.y_scale);
    return self;
}

Font *Font_create(const char *pattern, const FontOptions *options)
{
    if (Font_init() < 0) return 0;

    Font *cached = PSC_HashTable_get(byPattern,
	    pattern ? pattern : "<default>");
    if (cached)
    {
	Font_done();
	++cached->refcnt;
	return cached;
    }
    PSC_List *patterns = 0;
    PSC_ListIterator *pi = 0;
    if (pattern)
    {
	patterns = PSC_List_fromString(pattern, ",");
	pi = PSC_List_iterator(patterns);
    }

    int ismatch = 0;
    double pixelsize = defaultpixelsize;
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
	FcPattern *fcfont = FcFontMatch(0, fcpat, &result);
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
	FcChar8 *fontfile = 0;
	int fontindex = 0;
	if (ismatch)
	{
	    FcPatternGetString(fcfont, FC_FILE, 0, &fontfile);
	    FcPatternGetInteger(fcfont, FC_INDEX, 0, &fontindex);
	    if (!fontfile)
	    {
		PSC_Log_msg(PSC_L_WARNING, "Found font without a file");
		ismatch = 0;
	    }
	}
	if (ismatch)
	{
	    char *id = createFontId((const char *)fontfile,
		    fontindex, pixelsize);
	    cached = PSC_HashTable_get(byId, id);
	    if (cached)
	    {
		if (fcpat != defaultpat) FcPatternDestroy(fcpat);
		FcPatternDestroy(fcfont);
		free(id);
		Font_done();
		++cached->refcnt;
		return cached;
	    }

	    Font *self = createFromFile((const char *)fontfile, fontindex,
		    id, fcpat, options, pixelsize);
	    if (self)
	    {
		FcPatternDestroy(fcfont);
		PSC_ListIterator_destroy(pi);
		PSC_List_destroy(patterns);
		PSC_HashTable_set(byPattern,
			pattern ? pattern : "<default>", self, 0);
		if (id) PSC_HashTable_set(byId, id, self, 0);
		return self;
	    }
	    else
	    {
		if (fcpat != defaultpat) FcPatternDestroy(fcpat);
		FcPatternDestroy(fcfont);
		free(id);
		id = 0;
		ismatch = 0;
	    }
	}
    }
    PSC_ListIterator_destroy(pi);
    PSC_List_destroy(patterns);
    Font_done();
    PSC_Log_fmt(PSC_L_ERROR, "No matching font found for `%s'", pattern);
    return 0;
}

Font *Font_createVariant(Font *font, double pixelsize, FontStyle style,
	const FontOptions *options)
{
    if (Font_init() < 0) return 0;

    FcPattern *pattern = FcPatternDuplicate(font->pattern);
    FcPatternDel(pattern, FC_STYLE);
    FcPatternDel(pattern, FC_SIZE);
    FcPatternDel(pattern, FC_PIXEL_SIZE);
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternDel(pattern, FC_SLANT);
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixelsize);
    FcPatternAddInteger(pattern, FC_WEIGHT, (style & FS_BOLD)
	    ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
    FcPatternAddInteger(pattern, FC_SLANT, (style & FS_ITALIC)
	    ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);

    FcResult result;
    FcPattern *fcfont = FcFontMatch(0, pattern, &result);
    char *id = 0;
    if (result != FcResultMatch) goto error;

    FcChar8 *fontfile = 0;
    int fontindex = 0;
    FcPatternGetString(fcfont, FC_FILE, 0, &fontfile);
    FcPatternGetInteger(fcfont, FC_INDEX, 0, &fontindex);
    if (!fontfile)
    {
	PSC_Log_msg(PSC_L_WARNING, "Found font without a file");
	goto error;
    }

    id = createFontId((const char *)fontfile, fontindex, pixelsize);
    Font *cached = PSC_HashTable_get(byId, id);
    if (cached)
    {
	FcPatternDestroy(pattern);
	FcPatternDestroy(fcfont);
	free(id);
	Font_done();
	++cached->refcnt;
	return cached;
    }

    Font *self = createFromFile((const char *)fontfile, fontindex,
	    id, pattern, options, pixelsize);
    FcPatternDestroy(fcfont);
    if (self) return self;

error:
    FcPatternDestroy(fcfont);
    Font_done();
    free(id);
    FcPatternDestroy(pattern);
    ++font->refcnt;
    return font;
}

Font *Font_ref(Font *font)
{
    ++font->refcnt;
    return font;
}

FT_Face Font_face(const Font *self)
{
    return self->face;
}

FontGlyphType Font_glyphtype(const Font *self)
{
    return self->glyphtype;
}

double Font_pixelsize(const Font *self)
{
    return self->pixelsize;
}

double Font_fixedpixelsize(const Font *self)
{
    return self->fixedpixelsize;
}

uint16_t Font_linespace(const Font *self)
{
    return (self->face->size->metrics.height + 0x20) >> 6;
}

uint8_t Font_glyphidbits(const Font *self)
{
    return self->glyphidbits;
}

uint8_t Font_subpixelbits(const Font *self)
{
    return self->subpixelbits;
}

uint32_t Font_maxWidth(const Font *self)
{
    return self->maxWidth;
}

uint32_t Font_maxHeight(const Font *self)
{
    return self->maxHeight;
}

uint32_t Font_baseline(const Font *self)
{
    return self->baseline;
}

uint32_t Font_scale(const Font *self, uint32_t val)
{
    if (!self->fixedpixelsize) return val;
    double scale = self->pixelsize / self->fixedpixelsize;
    return val * scale + 1.;
}

int32_t Font_ftLoadFlags(const Font *self)
{
    int32_t loadflags = FT_LOAD_DEFAULT;
    switch (self->glyphtype)
    {
	case FGT_OUTLINE:	loadflags |= FT_LOAD_NO_BITMAP; break;
	case FGT_BITMAP_BGRA:	loadflags |= FT_LOAD_COLOR; break;
	default:		break;
    }
    return loadflags;
}

static const uint8_t mid[] = {
    1
};

static const uint8_t m3x3[] = {
    1, 2, 1,
    2, 3, 2,
    1, 2, 1
};

static const uint8_t m5x5[] = {
    1, 2, 2, 2, 1,
    2, 2, 3, 2, 2,
    2, 3, 4, 3, 2,
    2, 2, 3, 2, 2,
    1, 2, 2, 2, 1
};

static uint8_t fetch(const uint8_t *b, int stride, int w, int h,
	int x, int y, int nc, int c)
{
    if (x < 0) x = 0;
    if (x >= w) x = w-1;
    if (y < 0) y = 0;
    if (y >= h) y = h-1;
    return b[stride*y+nc*x+c];
}

static uint8_t filter(int k, const uint8_t *m, const uint8_t *b, int stride,
	int w, int h, int x, int y, int nc, int c)
{
    uint32_t num = 0;
    uint32_t den = 0;
    int off = k/2;
    for (int my = 0; my < k; ++my) for (int mx = 0; mx < k; ++mx)
    {
	uint8_t mv = m[k*my+mx];
	den += mv;
	num += mv * fetch(b, stride, w, h, x+mx-off, y+my-off, nc, c);
    }
    return ((num + den/2)/den) & 0xffU;
}

int Font_uploadGlyphs(Font *self, uint32_t ownerid,
	unsigned len, GlyphRenderInfo *glyphinfo)
{
    xcb_connection_t *c = 0;
    if (!self->glyphset)
    {
	c = X11Adapter_connection();
	self->glyphset = xcb_generate_id(c);
	if (self->glyphtype == FGT_BITMAP_BGRA)
	{
	    CHECK(xcb_render_create_glyph_set(c,
			self->glyphset, X11Adapter_argbformat()),
		    "Font: Cannot create glyphset for 0x%x",
		    (unsigned)ownerid);
	    self->maskglyphset = xcb_generate_id(c);
	    CHECK(xcb_render_create_glyph_set(c,
			self->maskglyphset, X11Adapter_alphaformat()),
		    "Font: Cannot create mask glyphset for 0x%x",
		    (unsigned)ownerid);
	}
	else
	{
	    CHECK(xcb_render_create_glyph_set(c,
			self->glyphset, X11Adapter_alphaformat()),
		    "Font: Cannot create glyphset for 0x%x",
		    (unsigned)ownerid);
	}
    }
    int rc = -1;
    unsigned toupload = 0;
    uint32_t *glyphids = PSC_malloc(len * sizeof *glyphids);
    xcb_render_glyphinfo_t *glyphs = 0;
    unsigned firstglyph = 0;
    uint8_t *bitmapdata = 0;
    uint8_t *maskdata = 0;
    size_t bitmapdatasz = 0;
    size_t maskdatasz = 0;
    uint32_t maxglyphid = self->glyphidmask | self->subpixelmask;
    for (unsigned i = 0; i < len; ++i)
    {
	if (glyphinfo[i].glyphid > maxglyphid) goto done;
	uint32_t word = glyphinfo[i].glyphid >> 5;
	uint32_t bit = 1U << (glyphinfo[i].glyphid & 0x1fU);
	if (self->uploaded[word] & bit) continue;
	glyphids[toupload++] = glyphinfo[i].glyphid;
    }
    if (!toupload)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Font: Nothing to upload for glyphset 0x%x",
		(unsigned)self->glyphset);
	rc = 0;
	goto done;
    }
    glyphs = PSC_malloc(toupload * sizeof *glyphs);
    memset(glyphs, 0, toupload * sizeof *glyphs);
    size_t bitmapdatapos = 0;
    size_t maskdatapos = 0;
    if (!c) c = X11Adapter_connection();
    int32_t loadflags = Font_ftLoadFlags(self);
    for (unsigned i = 0; i < toupload; ++i)
    {
	if (FT_Load_Glyph(self->face, glyphids[i] & self->glyphidmask,
		    loadflags) != 0) goto done;
	FT_GlyphSlot slot = self->face->glyph;
	int pixelsize = 1;
	if (self->glyphtype == FGT_OUTLINE)
	{
	    uint32_t xshift = glyphids[i] >> self->glyphidbits
		<< (6 - self->subpixelbits);
	    if (xshift)
	    {
		FT_Outline_Translate(&slot->outline, xshift, 0);
	    }
	}
	else if (self->glyphtype == FGT_BITMAP_BGRA)
	{
	    pixelsize = 4;
	}
	FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (slot->bitmap.buffer)
	{
	    glyphs[i].width = Font_scale(self, slot->bitmap.width);
	    glyphs[i].height = Font_scale(self, slot->bitmap.rows);
	}
	else
	{
	    glyphs[i].width = 0;
	    glyphs[i].height = 0;
	}
	glyphs[i].x = Font_scale(self, -slot->bitmap_left);
	glyphs[i].y = Font_scale(self, slot->bitmap_top);
	unsigned stride = (glyphs[i].width * pixelsize + 3) & ~3;
	size_t bitmapsz = stride * glyphs[i].height;
	if (glyphs[i].height == 0) bitmapsz = (pixelsize + 3) & ~3;
	unsigned maskstride = 0;
	size_t masksz = 0;
	if (self->glyphtype == FGT_BITMAP_BGRA)
	{
	    maskstride = (glyphs[i].width + 3) & ~3;
	    masksz = maskstride * glyphs[i].height;
	    if (glyphs[i].height == 0) masksz = 4;
	}
	if (sizeof (xcb_render_add_glyphs_request_t)
		+ (i - firstglyph) * (sizeof *glyphids + sizeof *glyphs)
		+ bitmapdatapos
		+ bitmapsz
		> X11Adapter_maxRequestSize())
	{
	    CHECK(xcb_render_add_glyphs(c, self->glyphset, i - firstglyph,
			glyphids + firstglyph, glyphs + firstglyph,
			bitmapdatapos, bitmapdata),
		    "Cannot upload to glyphset for 0x%x",
		    (unsigned)ownerid);
	    if (maskdatapos)
	    {
		CHECK(xcb_render_add_glyphs(c, self->maskglyphset,
			    i - firstglyph, glyphids + firstglyph,
			    glyphs + firstglyph, maskdatapos, maskdata),
			"Cannot upload to glyphset for 0x%x",
			(unsigned)ownerid);
	    }
	    for (unsigned j = firstglyph; j < i; ++j)
	    {
		uint32_t word = glyphids[j] >> 5;
		uint32_t bit = 1U << (glyphids[j] & 0x1fU);
		self->uploaded[word] |= bit;
	    }
	    bitmapdatapos = 0;
	    maskdatapos = 0;
	    firstglyph = i;
	}
	if (bitmapdatapos + bitmapsz > bitmapdatasz)
	{
	    bitmapdata = PSC_realloc(bitmapdata, bitmapdatapos + bitmapsz);
	    memset(bitmapdata + bitmapdatapos, 0, bitmapsz);
	    bitmapdatasz = bitmapdatapos + bitmapsz;
	}
	if (maskdatapos + masksz > maskdatasz)
	{
	    maskdata = PSC_realloc(maskdata, maskdatapos + masksz);
	    memset(maskdata + maskdatapos, 0, masksz);
	    maskdatasz = maskdatapos + masksz;
	}
	if (glyphs[i].height == 0)
	{
	    glyphs[i].height = 1;
	    glyphs[i].width = 1;
	}
	else if (self->glyphtype != FGT_OUTLINE)
	{
	    double scale;
	    if (self->fixedpixelsize)
	    {
		scale = self->fixedpixelsize / self->pixelsize;
	    }
	    else scale = 1.;
	    const uint8_t *m = m3x3;
	    int k = 3;
	    if (scale > 4)
	    {
		m = m5x5;
		k = 5;
	    }
	    else if (scale == 1.)
	    {
		m = mid;
		k = 1;
	    }
	    for (unsigned y = 0; y < glyphs[i].height; ++y)
	    {
		uint8_t *dst = bitmapdata + bitmapdatapos + y * stride;
		uint8_t *mask = maskdata + maskdatapos + y * maskstride;
		unsigned sy = scale * (double)y + scale / 2.;
		for (unsigned x = 0; x < glyphs[i].width; ++x)
		{
		    unsigned sx = scale * (double)x + scale / 2.;
		    if (self->glyphtype == FGT_BITMAP_BGRA)
		    {
			for (int b = 0; b < 4; ++b)
			{
			    dst[x*pixelsize+b] = filter(k, m,
				    slot->bitmap.buffer, slot->bitmap.pitch,
				    slot->bitmap.width, slot->bitmap.rows,
				    sx, sy, 4, b);
			}
			mask[x] = dst[x*pixelsize+3];
			dst[x*pixelsize+3] = 0xffU;
		    }
		    else dst[x] = filter(k, m,
			    slot->bitmap.buffer, slot->bitmap.pitch,
			    slot->bitmap.width, slot->bitmap.rows,
			    sx, sy, 1, 1);
		}
	    }
	}
	else
	{
	    for (unsigned y = 0; y < glyphs[i].height; ++y)
	    {
		memcpy(bitmapdata + bitmapdatapos + y * stride,
			slot->bitmap.buffer + y * slot->bitmap.pitch,
			glyphs[i].width * pixelsize);
	    }
	}
	bitmapdatapos += bitmapsz;
	maskdatapos += masksz;
    }
    CHECK(xcb_render_add_glyphs(c, self->glyphset, toupload - firstglyph,
		glyphids + firstglyph, glyphs + firstglyph,
		bitmapdatapos, bitmapdata),
	    "Cannot upload to glyphset for 0x%x", (unsigned)ownerid);
    if (maskdatapos)
    {
	CHECK(xcb_render_add_glyphs(c, self->maskglyphset,
		    toupload - firstglyph, glyphids + firstglyph,
		    glyphs + firstglyph, maskdatapos, maskdata),
		"Cannot upload to glyphset for 0x%x", (unsigned)ownerid);
    }
    for (unsigned i = firstglyph; i < toupload; ++i)
    {
	uint32_t word = glyphids[i] >> 5;
	uint32_t bit = 1U << (glyphids[i] & 0x1fU);
	self->uploaded[word] |= bit;
    }
    rc = 0;
done:
    free(maskdata);
    free(bitmapdata);
    free(glyphs);
    free(glyphids);
    return rc;
}

xcb_render_glyphset_t Font_glyphset(const Font *self)
{
    return self->glyphset;
}

xcb_render_glyphset_t Font_maskGlyphset(const Font *self)
{
    return self->maskglyphset;
}

void Font_destroy(Font *self)
{
    if (!self) return;
    if (--self->refcnt) return;
    if (self->glyphset)
    {
	xcb_connection_t *c = X11Adapter_connection();
	if (self->glyphtype == FGT_BITMAP_BGRA)
	{
	    xcb_render_free_glyph_set(c, self->maskglyphset);
	}
	xcb_render_free_glyph_set(c, self->glyphset);
    }
    FT_Done_Face(self->face);
    if (self->id) PSC_HashTable_delete(byId, self->id);
    const char *pat = 0;
    PSC_HashTableIterator *i = PSC_HashTable_iterator(byPattern);
    while (PSC_HashTableIterator_moveNext(i))
    {
	if (PSC_HashTableIterator_current(i) == self)
	{
	    pat = PSC_HashTableIterator_key(i);
	    break;
	}
    }
    PSC_HashTableIterator_destroy(i);
    if (pat) PSC_HashTable_delete(byPattern, pat);
    if (self->pattern != defaultpat) FcPatternDestroy(self->pattern);
    free(self->id);
    free(self);
    Font_done();
}

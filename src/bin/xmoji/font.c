#include "font.h"
#include "x11adapter.h"

#include <fontconfig/fontconfig.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

FT_Library ftlib;
int refcnt;

struct Font
{
    FT_Face face;
    double pixelsize;
};

typedef struct OutlineFont
{
    Font base;
    uint32_t uploaded[1U << 11U];
    xcb_render_glyphset_t glyphset;
    int failed;
    uint16_t uploading;
} OutlineFont;

typedef struct UploadCtx
{
    OutlineFont *of;
    uint16_t glyphid;
    uint8_t bitmap[];
} UploadCtx;

int Font_init(void)
{
    if (refcnt++) return 0;

    if (FcInit() != FcTrue)
    {
	--refcnt;
	PSC_Log_msg(PSC_L_ERROR, "Could not initialize fontconfig");
	return -1;
    }

    if (FT_Init_FreeType(&ftlib) != 0)
    {
	--refcnt;
	PSC_Log_msg(PSC_L_ERROR, "Could not initialize freetype");
	FcFini();
	return -1;
    }

    return 0;
}

void Font_done(void)
{
    if (--refcnt) return;
    FT_Done_FreeType(ftlib);
    FcFini();
}

static void create_glyphset_cb(void *ctx, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    OutlineFont *of = ctx;
    if (error)
    {
	PSC_Log_msg(PSC_L_ERROR, "Cannot create glyphset");
	of->failed = 1;
    }
}

static void add_glyph_cb(void *ctx, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    UploadCtx *uc = ctx;
    if (error)
    {
	PSC_Log_msg(PSC_L_ERROR, "Cannot upload glyph");
	uc->of->failed = 1;
    }
    else
    {
	--uc->of->uploading;
    }

    free(uc);
}

Font *Font_create(char **patterns)
{
    static char *emptypat[] = { 0 };
    char **p = patterns;
    if (!p) p = emptypat;
    char *patstr;
    FcPattern *fcfont;
    FT_Face face;
    double pixelsize = 0;
    double fixedpixelsize = 0;
    for (;;)
    {
	patstr = *p;
	if (!patstr) patstr = "sans";
	PSC_Log_fmt(PSC_L_DEBUG, "Looking for font: %s", patstr);
	FcPattern *fcpat = FcNameParse((FcChar8 *)patstr);
	FcConfigSubstitute(0, fcpat, FcMatchPattern);
	FcDefaultSubstitute(fcpat);
	FcResult result;
	fcfont = FcFontMatch(0, fcpat, &result);
	int ismatch = (result == FcResultMatch);
	FcChar8 *foundfamily = 0;
	if (ismatch) FcPatternGetString(fcfont, FC_FAMILY, 0, &foundfamily);
	if (ismatch && *p)
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
	pixelsize = 0;
	fixedpixelsize = 0;
	if (ismatch)
	{
	    FcPatternGetDouble(fcpat, FC_PIXEL_SIZE, 0, &pixelsize);
	    if (!pixelsize) ismatch = 0;
	}
	FcPatternDestroy(fcpat);
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
	    PSC_Log_fmt(PSC_L_DEBUG, "Font `%s:pixelsize=%.2f' found in `%s'",
		    (const char *)foundfamily,
		    fixedpixelsize ? fixedpixelsize : pixelsize,
		    (const char *)fontfile);
	}
	FcPatternDestroy(fcfont);
	fcfont = 0;
	if (ismatch) break;
	PSC_Log_fmt(PSC_L_WARNING, "No matching font found for `%s'", patstr);
	if (!*p) return 0;
	++p;
    }

    Font *self;
    if (fixedpixelsize)
    {
	self = PSC_malloc(sizeof *self);
	self->pixelsize = fixedpixelsize;
    }
    else
    {
	OutlineFont *of = PSC_malloc(sizeof *of);
	memset(of, 0, sizeof *of);
	xcb_connection_t *c = X11Adapter_connection();
	of->glyphset = xcb_generate_id(c);
	AWAIT(xcb_render_create_glyph_set(c,
		    of->glyphset, X11Adapter_alphaformat()),
		of, create_glyphset_cb);
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

int Font_uploadGlyph(Font *self, uint32_t glyphid)
{
    if (glyphid > 0xffffU) return -1;
    if (!(self->face->face_flags & FT_FACE_FLAG_SCALABLE)) return -1;
    OutlineFont *of = (OutlineFont *)self;
    uint32_t word = glyphid >> 5;
    uint32_t bit = 1U << (glyphid & 0x1fU);
    if (of->uploaded[word] & bit) return 0;
    if (FT_Load_Glyph(self->face, glyphid, FT_LOAD_RENDER) != 0) return -1;
    FT_GlyphSlot slot = self->face->glyph;
    xcb_render_glyphinfo_t glyph;
    glyph.x = -slot->bitmap_left;
    glyph.y = slot->bitmap_top;
    glyph.width = slot->bitmap.width;
    glyph.height = slot->bitmap.rows;
    glyph.x_off = 0;
    glyph.y_off = 0;
    int stride = (glyph.width+3)&~3;
    UploadCtx *ctx = PSC_malloc(sizeof *ctx + stride*glyph.height);
    ctx->of = of;
    ctx->glyphid = glyphid;
    memset(ctx->bitmap, 0, stride*glyph.height);
    for (int y = 0; y < glyph.height; ++y)
    {
	memcpy(ctx->bitmap+y*stride, slot->bitmap.buffer+y*glyph.width,
		glyph.width);
    }
    AWAIT(xcb_render_add_glyphs(X11Adapter_connection(),
		of->glyphset, 1, &glyphid, &glyph, stride*glyph.height,
		ctx->bitmap),
	    ctx, add_glyph_cb);
    of->uploaded[word] |= bit;
    ++of->uploading;
    return 0;
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
    FT_Done_Face(self->face);
    free(self);
}

#include "font.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

FT_Library ftlib;
int refcnt;

struct Font
{
    FT_Face face;
};

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

Font *Font_create(char **patterns)
{
    static char *emptypat[] = { 0 };
    char **p = patterns;
    if (!p) p = emptypat;
    char *patstr;
    FcPattern *fcfont;
    FT_Face face;
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
	double pixelsize = 0;
	double fixedpixelsize = 0;
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
	FcPatternDestroy(fcfont);
	fcfont = 0;
	if (ismatch)
	{
	    PSC_Log_fmt(PSC_L_DEBUG, "Font `%s:pixelsize=%.2f' found in `%s'",
		    (const char *)foundfamily,
		    fixedpixelsize ? fixedpixelsize : pixelsize,
		    (const char *)fontfile);
	    break;
	}
	PSC_Log_fmt(PSC_L_WARNING, "No matching font found for `%s'", patstr);
	if (!*p) return 0;
	++p;
    }

    Font *self = PSC_malloc(sizeof *self);
    self->face = face;
    return self;
}

void Font_destroy(Font *self)
{
    if (!self) return;
    FT_Done_Face(self->face);
    free(self);
}

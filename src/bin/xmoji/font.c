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
	FcValue reqfamily;
	FcPatternGet(fcpat, FC_FAMILY, 0, &reqfamily);
	FcPatternDestroy(fcpat);
	FcValue foundfamily;
	FcPatternGet(fcfont, FC_FAMILY, 0, &foundfamily);
	if (result == FcResultMatch && (!*p || !strcmp(
		    (const char *)reqfamily.u.s,
		    (const char *)foundfamily.u.s))) break;
	FcPatternDestroy(fcfont);
	fcfont = 0;
	if (!*p) break;
	++p;
    }

    if (!fcfont)
    {
	PSC_Log_fmt(PSC_L_WARNING, "No matching font found for `%s'", patstr);
	return 0;
    }

    FcValue fontfile;
    FcValue fontindex;
    FcResult result = FcPatternGet(fcfont, FC_FILE, 0, &fontfile);
    if (result != FcResultMatch)
    {
	FcPatternDestroy(fcfont);
	PSC_Log_msg(PSC_L_WARNING, "Found font without a file");
	return 0;
    }
    char *name = (char *)FcPatternFormat(fcfont,
	    (FcChar8 *)"%{family}%{:pixelsize=}");
    PSC_Log_fmt(PSC_L_DEBUG, "Font `%s' found in `%s'", name,
	    (const char *)fontfile.u.s);
    free(name);
    result = FcPatternGet(fcfont, FC_INDEX, 0, &fontindex);
    if (result != FcResultMatch)
    {
	fontindex.type = FcTypeInteger;
	fontindex.u.i = 0;
    }

    Font *self = PSC_malloc(sizeof *self);
    FT_New_Face(ftlib, (const char *)fontfile.u.s, fontindex.u.i, &self->face);
    return self;
}

void Font_destroy(Font *self)
{
    if (!self) return;
    FT_Done_Face(self->face);
    free(self);
}

#include "colorset.h"
#include "x11adapter.h"
#include "xrdb.h"

#include <errno.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

#define TARGETSCHUNKSZ 8

struct ColorSet
{
    Color colors[COLOR_NUMROLES];
    unsigned npending;
};

typedef struct ColorLookupTarget
{
    ColorSet *colorSet;
    ColorRole role;
} ColorLookupTarget;

typedef struct ColorLookup
{
    const char *name;
    ColorLookupTarget *targets;
    size_t targetslen;
    size_t targetscapa;
} ColorLookup;

static const char *reskeys[COLOR_NUMROLES][2] = {
    { "Foreground", "foreground" },
    { "Background", "background" },
    { "Foreground", "aboveForeground" },
    { "Background", "aboveBackground" },
    { "Foreground", "belowForeground" },
    { "Background", "belowBackground" },
    { "Foreground", "activeForeground" },
    { "Background", "activeBackground" },
    { "Foreground", "disabledForeground" },
    { "Background", "disabledBackground" },
    { "Foreground", "selectedForeground" },
    { "Background", "selectedBackground" },
    { "Foreground", "tooltipForeground" },
    { "Background", "tooltipBackground" },
    { "Border", "tooltipBorder" }
};
static int refcnt;
static PSC_HashTable *colorCache;
static PSC_HashTable *lookups;

static const ColorSet defcolors = {
    .colors = {		// Color role	    X11 color name
	0x000000ff,	// NORMAL	    black
	0xbebebeff,	// BG_NORMAL	    gray
	0x000000ff,	// ABOVE	    black
	0xd3d3d3ff,	// BG_ABOVE	    light gray
	0x000000ff,	// BELOW	    black
	0xa9a9a9ff,	// BG_BELOW	    dark gray
	0x2f4f4fff,	// ACTIVE	    dark slate gray
	0xadd8e6ff,	// BG_ACTIVE	    light blue
	0x696969ff,	// DISABLED	    dim gray
	0xc0c0c0ff,	// BG_DISABLED	    silver
	0xadd8e6ff,	// SELECTED	    light blue
	0x0000cdff,	// BG_SELECTED	    medium blue
	0x000000ff,	// TOOLTIP	    black
	0xffdeadff,	// BG_TOOLTIP	    navajo white
	0x000000ff	// BORDER_TOOLTIP   black
    },	// ref: https://en.wikipedia.org/wiki/X11_color_names
    .npending = 0
};

void destroyLookup(void *obj)
{
    if (!obj) return;
    ColorLookup *lookup = obj;
    free(lookup->targets);
    free(lookup);
}

static void colorReceived(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    ColorLookup *self = obj;
    uintptr_t colorval = 0;
    if (reply && !error)
    {
	xcb_lookup_color_reply_t *color = reply;
	colorval = ((color->exact_red & 0xff00) << 16)
	    | ((color->exact_green & 0xff00) << 8)
	    | (color->exact_blue & 0xff00) | 0xffU;
    }
    for (size_t i = 0; i < self->targetslen; ++i)
    {
	--self->targets[i].colorSet->npending;
	self->targets[i].colorSet->colors[self->targets[i].role] = colorval;
    }
    if (!colorval) colorval = 1;
    if (!colorCache) colorCache = PSC_HashTable_create(6);
    PSC_HashTable_set(colorCache, self->name, (void *)colorval, 0);
    PSC_HashTable_delete(lookups, self->name);
}

static void lookupColor(ColorSet *self, const char *name, ColorRole role)
{
    if (!lookups) lookups = PSC_HashTable_create(4);
    ColorLookup *lookup = PSC_HashTable_get(lookups, name);
    if (!lookup)
    {
	lookup = PSC_malloc(sizeof *lookup);
	lookup->name = name;
	lookup->targets = PSC_malloc(TARGETSCHUNKSZ * sizeof *lookup->targets);
	lookup->targetslen = 0;
	lookup->targetscapa = TARGETSCHUNKSZ;
	PSC_HashTable_set(lookups, name, lookup, destroyLookup);
	AWAIT(xcb_lookup_color(X11Adapter_connection(),
		    X11Adapter_screen()->default_colormap, strlen(name), name),
		lookup, colorReceived);
    }
    if (lookup->targetslen == lookup->targetscapa)
    {
	lookup->targetscapa += TARGETSCHUNKSZ;
	lookup->targets = PSC_realloc(lookup->targets,
		lookup->targetscapa * sizeof *lookup->targets);
    }
    lookup->targets[lookup->targetslen].colorSet = self;
    lookup->targets[lookup->targetslen].role = role;
    ++lookup->targetslen;
    ++self->npending;
}

static void ColorSet_init(void)
{
    if (refcnt++) return;
    XRdb *rdb = X11Adapter_resources();
    for (unsigned i = 0; i < COLOR_NUMROLES; ++i)
    {
	XRdb_register(rdb, reskeys[i][0], reskeys[i][1]);
    }
}

static void ColorSet_done(void)
{
    if (--refcnt) return;
    PSC_HashTable_destroy(lookups);
    PSC_HashTable_destroy(colorCache);
    lookups = 0;
    colorCache = 0;
}

const ColorSet *ColorSet_default(void)
{
    return &defcolors;
}

ColorSet *ColorSet_create(void)
{
    ColorSet_init();
    ColorSet *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    return self;
}

ColorSet *ColorSet_createFor(const char *name)
{
    ColorSet *self = ColorSet_create();
    XRdb *rdb = X11Adapter_resources();
    for (unsigned i = 0; i < COLOR_NUMROLES; ++i)
    {
	const char *colorstr = XRdb_value(rdb,
		XRdbKey(name, reskeys[i][1]), XRQF_OVERRIDES);
	if (!colorstr) continue;
	if (Color_fromString(self->colors + i, colorstr) < 0)
	{
	    // Ask cache or server for a named X11 color
	    uintptr_t cached = 0;
	    if (colorCache) cached = (uintptr_t)PSC_HashTable_get(
		    colorCache, colorstr);
	    if (cached == 1) continue;
	    else if (cached)
	    {
		self->colors[i] = cached;
		continue;
	    }
	    lookupColor(self, colorstr, i);
	}
    }
    return self;
}

ColorSet *ColorSet_clone(const ColorSet *self)
{
    ColorSet *clone = PSC_malloc(sizeof *clone);
    memcpy(clone, self, sizeof *clone);
    return clone;
}

int ColorSet_valid(const ColorSet *self)
{
    return !self->npending;
}

Color ColorSet_color(const ColorSet *self, ColorRole role)
{
    return self->colors[role];
}

void ColorSet_setColor(ColorSet *self, ColorRole role, Color color)
{
    self->colors[role] = color;
}

void ColorSet_destroy(ColorSet *self)
{
    free(self);
    ColorSet_done();
}

#define chanmask(b) ((1U<<(b))-1)
#define convert(b, shift, val) \
    ((((val)>>(shift))&(chanmask(b)))*0xffU/(chanmask(b)))
#define combine(r,g,b,a) (((r)<<24)|((g)<<16)|(((b)<<8))|(a))
#define colorconv(b,cv) combine(convert((b),(b)*2,(cv)),\
	convert((b),(b),(cv)), convert((b),0,(cv)), 0xffU)
#define colorconva(b,cv) combine(convert((b),(b)*3,(cv)),\
	convert((b),(b)*2,(cv)), convert((b),(b),(cv)), convert((b),0,(cv)))
#define skipws(p) while (*(p) == ' ' || *(p) == '\t') ++(p)

int Color_fromString(Color *color, const char *str)
{
    if (!str) return -1;
    skipws(str);
    if (!*str) return -1;
    if (*str == '#')
    {
	// Hex formats '#rgb', '#rrggbb', '#rrrgggbbb', '#rrrrggggbbbb',
	// '#rgba', '#rrggbbaa', and '#rrrrggggbbbbaaaa'
	char *endp = 0;
	errno = 0;
	unsigned long long cv = strtoull(str+1, &endp, 16);
	if (errno != 0 || endp == str+1) return -1;
	char *p = endp;
	skipws(p);
	if (*p) return -1;
	if (endp - str == 17)
	{
	    *color = colorconva(16, cv);
	    return 0;
	}
	if (endp - str == 13)
	{
	    *color = colorconv(16, cv);
	    return 0;
	}
	if (endp - str == 10)
	{
	    *color = colorconv(12, cv);
	    return 0;
	}
	if (endp - str == 9)
	{
	    *color = cv;
	    return 0;
	}
	if (endp - str == 7)
	{
	    *color = (cv << 8) | 0xffU;
	    return 0;
	}
	if (endp - str == 5)
	{
	    *color = colorconva(4, cv);
	    return 0;
	}
	if (endp - str == 4)
	{
	    *color = colorconv(4, cv);
	    return 0;
	}
	return -1;
    }
    int ishex = 0;
    int isdec = 0;
    int hasalpha = 0;
    const char *p = 0;
    if (*str == '(')
    {
	p = str + 1;
	isdec = 1;
    }
    else if (!strncmp(str, "rgba:", sizeof "rgba:" - 1))
    {
	p = str + 5;
	ishex = 1;
	hasalpha = 1;
    }
    else if (!strncmp(str, "rgb:", sizeof "rgb:" - 1))
    {
	p = str + 4;
	ishex = 1;
    }
    else if (!strncmp(str, "rgba(", sizeof "rgba(" - 1))
    {
	p = str + 5;
	isdec = 1;
	hasalpha = 1;
    }
    else if (!strncmp(str, "rgb(", sizeof "rgb(" - 1))
    {
	p = str + 4;
	isdec = 1;
    }
    if (ishex)
    {
	// Hex formats 'rgb:r*/g*/b*' and 'rgba:r*/g*/b*/a*'
	char *endp = 0;
	errno = 0;
	unsigned long chan = strtoul(p, &endp, 16);
	if (errno != 0 || endp == p || endp - p > 4 || *endp != '/') return -1;
	uint8_t red = convert((endp-p)*4, 0, chan);
	p = endp + 1;
	chan = strtoul(p, &endp, 16);
	if (errno != 0 || endp == p || endp - p > 4 || *endp != '/') return -1;
	uint8_t green = convert((endp-p)*4, 0, chan);
	p = endp + 1;
	chan = strtoul(p, &endp, 16);
	if (errno != 0 || endp == p || endp - p > 4) return -1;
	uint8_t blue = convert((endp-p)*4, 0, chan);
	uint8_t alpha = 0xffU;
	if (hasalpha)
	{
	    if (*endp != '/') return -1;
	    p = endp + 1;
	    chan = strtoul(p, &endp, 16);
	    if (errno != 0 || endp == p || endp - p > 4) return -1;
	    alpha = convert((endp-p)*4, 0, chan);
	}
	p = endp;
	skipws(p);
	if (*p) return -1;
	*color = combine(red, green, blue, alpha);
	return 0;
    }
    if (isdec)
    {
	// Decimal formats '(r, g, b)', 'rgb(r, g, b)' and 'rgba(r, g, b, a)'
	// (borrowed from CSS)
	skipws(p);
	if (!*p) return -1;
	char *endp = 0;
	errno = 0;
	unsigned long red = strtoul(p, &endp, 10);
	if (errno != 0 || endp == p || red > 255) return -1;
	p = endp;
	skipws(p);
	if (*p != ',') return -1;
	++p;
	skipws(p);
	if (!*p) return -1;
	unsigned long green = strtoul(p, &endp, 10);
	if (errno != 0 || endp == p || green > 255) return -1;
	p = endp;
	skipws(p);
	if (*p != ',') return -1;
	++p;
	skipws(p);
	if (!*p) return -1;
	unsigned long blue = strtoul(p, &endp, 10);
	if (errno != 0 || endp == p || blue > 255) return -1;
	p = endp;
	skipws(p);
	unsigned long alpha = 255;
	if (hasalpha)
	{
	    if (*p != ',') return -1;
	    ++p;
	    skipws(p);
	    if (!*p) return -1;
	    alpha = strtoul(p, &endp, 10);
	    if (errno != 0 || endp == p || alpha > 255) return -1;
	    p = endp;
	    skipws(p);
	}
	if (*p != ')') return -1;
	++p;
	skipws(p);
	if (*p) return -1;
	*color = combine(red, green, blue, alpha);
	return 0;
    }
    return -1;
}


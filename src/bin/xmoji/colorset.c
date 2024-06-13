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
    { "Background", "selectedBackground" }
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
	0x0000cdff	// BG_SELECTED	    medium blue
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
    PSC_HashTable_set(colorCache, self->name, (void *)colorval, 0);
    PSC_HashTable_delete(lookups, self->name);
}

static void lookupColor(ColorSet *self, const char *name, ColorRole role)
{
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
    colorCache = PSC_HashTable_create(6);
    lookups = PSC_HashTable_create(4);
}

static void ColorSet_done(void)
{
    if (--refcnt) return;
    PSC_HashTable_destroy(lookups);
    PSC_HashTable_destroy(colorCache);
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
	if (*colorstr == '#')
	{
	    // Hex formats '#rrggbb' and '#rgb'
	    char *endp = 0;
	    errno = 0;
	    unsigned long cv = strtoul(colorstr+1, &endp, 16);
	    if (errno != 0 || endp == colorstr+1 || *endp) continue;
	    if (endp - colorstr == 7)
	    {
		self->colors[i] = (cv << 8) | 0xffU;
	    }
	    else if (endp - colorstr == 4)
	    {
		uint8_t red = (cv >> 8) & 0xf;
		red |= red << 4;
		uint8_t green = (cv >> 4) & 0xf;
		green |= green << 4;
		uint8_t blue = cv & 0xf;
		blue |= blue << 4;
		self->colors[i] =
		    (red << 24) | (green << 16) | (blue << 8) | 0xffU;
	    }
	    else continue;
	}
	else if (*colorstr == '(')
	{
	    // Decimal format '(r, g, b)'
	    const char *p = colorstr+1;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (!*p) continue;
	    char *endp = 0;
	    errno = 0;
	    unsigned long red = strtoul(p, &endp, 10);
	    if (errno != 0 || endp == p || red > 255) continue;
	    p = endp;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (*p != ',') continue;
	    ++p;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (!*p) continue;
	    unsigned long green = strtoul(p, &endp, 10);
	    if (errno != 0 || endp == p || green > 255) continue;
	    p = endp;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (*p != ',') continue;
	    ++p;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (!*p) continue;
	    unsigned long blue = strtoul(p, &endp, 10);
	    if (errno != 0 || endp == p || blue > 255) continue;
	    p = endp;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (*p != ')') continue;
	    ++p;
	    while (*p && (*p == ' ' || *p == '\t')) ++p;
	    if (*p) continue;
	    self->colors[i] =
		(red << 24) | (green << 16) | (blue << 8) | 0xffU;
	}
	else
	{
	    // Named X11 color, ask the server
	    uintptr_t cached = (uintptr_t)PSC_HashTable_get(
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


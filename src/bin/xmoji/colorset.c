#include "colorset.h"
#include "x11adapter.h"
#include "xrdb.h"

#include <errno.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct ColorSet
{
    Color colors[COLOR_NUMROLES];
};

static const char *reskeys[COLOR_NUMROLES][2] = {
    { "Foreground", "foreground" },
    { "Background", "background" },
    { "Foreground", "activeForeground" },
    { "Background", "activeBackground" },
    { "Foreground", "disabledForeground" },
    { "Background", "disabledBackground" },
    { "Foreground", "selectedForeground" },
    { "Background", "selectedBackground" }
};
static int keysregistered;

static const ColorSet defcolors = {
    .colors = {
	0x000000ff,
	0xbbbbbbff,
	0x004444ff,
	0xffffffff,
	0x666666ff,
	0xaaaaaaff,
	0xffffffff,
	0x003399ff
    }
};

const ColorSet *ColorSet_default(void)
{
    return &defcolors;
}

ColorSet *ColorSet_create(void)
{
    ColorSet *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    return self;
}

ColorSet *ColorSet_createFor(const char *name)
{
    ColorSet *self = ColorSet_create();
    XRdb *rdb = X11Adapter_resources();
    if (!keysregistered)
    {
	for (unsigned i = 0; i < COLOR_NUMROLES; ++i)
	{
	    XRdb_register(rdb, reskeys[i][0], reskeys[i][1]);
	}
	keysregistered = 1;
    }
    for (unsigned i = 0; i < COLOR_NUMROLES; ++i)
    {
	const char *colorstr = XRdb_value(rdb, XRdbKey(name, reskeys[i][1]));
	if (!colorstr || *colorstr != '#') continue;
	char *endp = 0;
	errno = 0;
	unsigned long c = strtoul(colorstr+1, &endp, 16);
	if (errno != 0 || endp == colorstr+1 || *endp) continue;
	self->colors[i] = (c << 8) | 0xffU;
    }
    return self;
}

ColorSet *ColorSet_clone(const ColorSet *self)
{
    ColorSet *clone = PSC_malloc(sizeof *clone);
    memcpy(clone, self, sizeof *clone);
    return clone;
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
}


#include "colorset.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct ColorSet
{
    Color colors[COLOR_NUMROLES];
};

ColorSet *ColorSet_create(Color bg, Color fg)
{
    ColorSet *self = PSC_malloc(sizeof *self);
    for (int i = 0; i < COLOR_NUMROLES; i += 2)
    {
	self->colors[i] = fg;
	self->colors[i+1] = bg;
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


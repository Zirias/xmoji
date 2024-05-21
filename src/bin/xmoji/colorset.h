#ifndef XMOJI_COLORSET_H
#define XMOJI_COLORSET_H

#include "valuetypes.h"

#include <poser/decl.h>

typedef enum ColorRole
{
    COLOR_NORMAL,
    COLOR_BG_NORMAL,
    COLOR_ACTIVE,
    COLOR_BG_ACTIVE,
    COLOR_INACTIVE,
    COLOR_BG_INACTIVE,
    COLOR_SELECTED,
    COLOR_BG_SELECTED,
    COLOR_NUMROLES
} ColorRole;

C_CLASS_DECL(ColorSet);

ColorSet *ColorSet_create(Color bg, Color fg);
ColorSet *ColorSet_clone(const ColorSet *self) CMETHOD;
Color ColorSet_color(const ColorSet *self, ColorRole role) CMETHOD;
void ColorSet_setColor(ColorSet *self, ColorRole role, Color color) CMETHOD;
void ColorSet_destroy(ColorSet *self);

#endif

#ifndef XMOJI_COLORSET_H
#define XMOJI_COLORSET_H

#include "valuetypes.h"

#include <poser/decl.h>

typedef enum ColorRole
{
    COLOR_NORMAL,
    COLOR_BG_NORMAL,
    COLOR_ABOVE,
    COLOR_BG_ABOVE,
    COLOR_BELOW,
    COLOR_BG_BELOW,
    COLOR_ACTIVE,
    COLOR_BG_ACTIVE,
    COLOR_DISABLED,
    COLOR_BG_DISABLED,
    COLOR_SELECTED,
    COLOR_BG_SELECTED,
    COLOR_TOOLTIP,
    COLOR_BG_TOOLTIP,
    COLOR_BORDER,
    COLOR_BORDER_TOOLTIP,
    COLOR_NUMROLES
} ColorRole;

C_CLASS_DECL(ColorSet);

const ColorSet *ColorSet_default(void);
ColorSet *ColorSet_create(void);
ColorSet *ColorSet_createFor(const char *name);
ColorSet *ColorSet_clone(const ColorSet *self) CMETHOD;
int ColorSet_valid(const ColorSet *self) CMETHOD;
Color ColorSet_color(const ColorSet *self, ColorRole role) CMETHOD;
void ColorSet_setColor(ColorSet *self, ColorRole role, Color color) CMETHOD;
void ColorSet_destroy(ColorSet *self);

int Color_fromString(Color *color, const char *str);

#endif

#ifndef XMOJI_TOOLTIP_H
#define XMOJI_TOOLTIP_H

#include "valuetypes.h"

#include <poser/decl.h>

C_CLASS_DECL(Tooltip);
C_CLASS_DECL(UniStr);
C_CLASS_DECL(Window);

Tooltip *Tooltip_create(const UniStr *text, unsigned delay);
void Tooltip_activate(Tooltip *self, Window *window)
    CMETHOD ATTR_NONNULL((2));
void Tooltip_cancel(Tooltip *self)
    CMETHOD;
void Tooltip_destroy(Tooltip *self);

#endif

#ifndef XMOJI_PEN_H
#define XMOJI_PEN_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(Pen);

Pen *Pen_create(void) ATTR_RETNONNULL;
void Pen_configure(Pen *self, PictFormat format, Color color) CMETHOD;
xcb_render_picture_t Pen_picture(Pen *self,
	xcb_render_picture_t ownerpic) CMETHOD;
void Pen_destroy(Pen *self);

#endif

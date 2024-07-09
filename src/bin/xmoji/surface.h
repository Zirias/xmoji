#ifndef XMOJI_SURFACE_H
#define XMOJI_SURFACE_H

#include "widget.h"

typedef struct MetaSurface
{
    MetaWidget base;
} MetaSurface;

#define MetaSurface_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(Surface);

Surface *Surface_createBase(void *derived, void *parent);
#define Surface_create(...) Surface_createBase(0, __VA_ARGS__)
void Surface_setWidget(void *self, void *widget) CMETHOD;
void Surface_render(void *self, xcb_render_picture_t picture,
	Pos pos, Rect rect) CMETHOD;

#endif

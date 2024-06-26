#ifndef XMOJI_PIXMAP_H
#define XMOJI_PIXMAP_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(Pixmap);

Pixmap *Pixmap_createFromPng(const unsigned char *data, size_t datasz);
Pixmap *Pixmap_ref(Pixmap *pixmap) ATTR_NONNULL((1));
Size Pixmap_size(const Pixmap *self) CMETHOD;
const unsigned char *Pixmap_bitmap(const Pixmap *self, size_t *size);
void Pixmap_render(Pixmap *self, xcb_render_picture_t picture, Pos pos)
    CMETHOD;
void Pixmap_destroy(Pixmap *self);

#endif

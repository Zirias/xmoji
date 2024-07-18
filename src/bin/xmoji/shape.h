#ifndef XMOJI_SHAPE_H
#define XMOJI_SHAPE_H

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(Shape);

typedef xcb_render_picture_t (*ShapeRenderer)(
	xcb_render_picture_t ownerpic, const void *data);

Shape *Shape_create(ShapeRenderer renderer,
	size_t datasz, const void *data) ATTR_RETNONNULL;
void Shape_render(Shape *self, xcb_render_picture_t ownerpic) CMETHOD;
xcb_render_picture_t Shape_picture(const Shape *self) CMETHOD ATTR_PURE;
void Shape_destroy(Shape *self);

#endif

#include "shape.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct Shape
{
    ShapeRenderer renderer;
    const void *data;
    size_t datasz;
    xcb_render_picture_t picture;
    unsigned refcnt;
};

static PSC_List *shapes;

Shape *Shape_create(ShapeRenderer renderer, size_t datasz, const void *data)
{
    if (!shapes) shapes = PSC_List_create();
    for (size_t i = 0; i < PSC_List_size(shapes); ++i)
    {
	Shape *shape = PSC_List_at(shapes, i);
	if (shape->renderer == renderer && shape->datasz == datasz
		&& !memcmp(shape->data, data, datasz))
	{
	    ++shape->refcnt;
	    return shape;
	}
    }
    Shape *self = PSC_malloc(sizeof *self);
    self->renderer = renderer;
    self->data = data;
    self->datasz = datasz;
    self->picture = 0;
    self->refcnt = 1;
    PSC_List_append(shapes, self, 0);
    return self;
}

void Shape_render(Shape *self, xcb_render_picture_t ownerpic)
{
    if (self->picture) return;
    self->picture = self->renderer(ownerpic, self->data);
}

xcb_render_picture_t Shape_picture(const Shape *self)
{
    return self->picture;
}

void Shape_destroy(Shape *self)
{
    if (!self) return;
    if (--self->refcnt) return;
    PSC_List_remove(shapes, self);
    if (!PSC_List_size(shapes))
    {
	PSC_List_destroy(shapes);
	shapes = 0;
    }
    if (self->picture)
    {
	xcb_render_free_picture(X11Adapter_connection(), self->picture);
    }
    free(self);
}


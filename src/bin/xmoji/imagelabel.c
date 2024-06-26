#include "imagelabel.h"

#include "pixmap.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);

static MetaImageLabel mo = MetaImageLabel_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, minSize, 0, 0, 0,
	"ImageLabel", destroy);

struct ImageLabel
{
    Object base;
    Pixmap *pixmap;
    Size minSize;
};

static void destroy(void *obj)
{
    ImageLabel *self = obj;
    Pixmap_destroy(self->pixmap);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    ImageLabel *self = Object_instance(obj);
    if (!picture || !self->pixmap) return 0;
    Pos pos = Widget_contentOrigin(self, self->minSize);
    Pixmap_render(self->pixmap, picture, pos);
    return 0;
}

static Size minSize(const void *obj)
{
    const ImageLabel *self = Object_instance(obj);
    return self->minSize;
}

ImageLabel *ImageLabel_createBase(void *derived, const char *name, void *parent)
{
    ImageLabel *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->pixmap = 0;
    self->minSize = (Size){0, 0};

    return self;
}

Pixmap *ImageLabel_pixmap(const void *self)
{
    const ImageLabel *l = Object_instance(self);
    return l->pixmap;
}

void ImageLabel_setPixmap(void *self, Pixmap *pixmap)
{
    ImageLabel *l = Object_instance(self);
    Pixmap_destroy(l->pixmap);
    l->pixmap = Pixmap_ref(pixmap);
    Size oldSize = l->minSize;
    l->minSize = Pixmap_size(pixmap);
    if (oldSize.width != l->minSize.width
	    || oldSize.height != l->minSize.height)
    {
	Widget_requestSize(l);
    }
}


#include "icon.h"

#include "pixmap.h"
#include "window.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>

struct Icon
{
    PSC_List *pixmaps;
    unsigned refcnt;
};

static void destroyPixmap(void *obj)
{
    Pixmap_destroy(obj);
}

Icon *Icon_create(void)
{
    Icon *self = PSC_malloc(sizeof *self);
    self->pixmaps = PSC_List_create();
    self->refcnt = 1;
    return self;
}

Icon *Icon_ref(Icon *icon)
{
    ++icon->refcnt;
    return icon;
}

void Icon_add(Icon *self, Pixmap *pixmap)
{
    PSC_List_append(self->pixmaps, Pixmap_ref(pixmap), destroyPixmap);
}

void Icon_apply(Icon *self, Window *window)
{
    size_t n = PSC_List_size(self->pixmaps);
    xcb_window_t w = Window_id(window);
    xcb_connection_t *c = X11Adapter_connection();
    if (!n)
    {
	CHECK(xcb_delete_property(c, w, A(_NET_WM_ICON)),
		"Cannot delete icon from 0x%x", (unsigned)w);
	return;
    }
    size_t propsz = 2 * n * sizeof(uint32_t);
    PSC_ListIterator *i = PSC_List_iterator(self->pixmaps);
    size_t bitmapsz;
    const unsigned char *bitmap;
    while (PSC_ListIterator_moveNext(i))
    {
	Pixmap *p = PSC_ListIterator_current(i);
	bitmap = Pixmap_bitmap(p, &bitmapsz);
	propsz += bitmapsz;
    }
    uint32_t *propval = PSC_malloc(propsz);
    uint32_t *propp = propval;
    while (PSC_ListIterator_moveNext(i))
    {
	Pixmap *p = PSC_ListIterator_current(i);
	Size dim = Pixmap_size(p);
	*propp++ = dim.width;
	*propp++ = dim.height;
	bitmap = Pixmap_bitmap(p, &bitmapsz);
	memcpy(propp, bitmap, bitmapsz);
	propp += bitmapsz >> 2;
    }
    PSC_ListIterator_destroy(i);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, A(_NET_WM_ICON),
		XCB_ATOM_CARDINAL, 32, propsz >> 2, propval),
	    "Cannot set icon on 0x%x", (unsigned)w);
    free(propval);
}

void Icon_destroy(Icon *self)
{
    if (!self) return;
    if (--self->refcnt) return;
    PSC_List_destroy(self->pixmaps);
    free(self);
}


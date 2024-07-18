#include "pen.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

typedef struct PenEntry
{
    PictFormat format;
    Color color;
    xcb_render_picture_t picture;
    unsigned refcnt;
} PenEntry;

struct Pen
{
    PenEntry *entry;
    PictFormat format;
    Color color;
};

static PSC_List *entries;
static unsigned refcnt;

void deleteEntry(void *p)
{
    if (!p) return;
    PenEntry *entry = p;
    if (entry->picture)
    {
	xcb_render_free_picture(X11Adapter_connection(), entry->picture);
    }
    free(entry);
}

Pen *Pen_create(void)
{
    if (!refcnt++) entries = PSC_List_create();
    Pen *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    return self;
}

void Pen_configure(Pen *self, PictFormat format, Color color)
{
    if (self->format == format && self->color == color) return;
    self->format = format;
    self->color = color;
    if (self->entry) --self->entry->refcnt;
    self->entry = 0;
}

xcb_render_picture_t Pen_picture(Pen *self,
	xcb_render_picture_t ownerpic)
{
    if (self->entry) return self->entry->picture;

    PenEntry *avail = 0;
    for (size_t i = 0; i < PSC_List_size(entries); ++i)
    {
	PenEntry *entry = PSC_List_at(entries, i);
	if (entry->format != self->format) continue;
	if (entry->color == self->color)
	{
	    ++entry->refcnt;
	    self->entry = entry;
	    return self->entry->picture;
	}
	else if (!avail && !entry->refcnt) avail = entry;
    }
    if (!avail)
    {
	avail = PSC_malloc(sizeof *avail);
	avail->format = self->format;
	avail->picture = 0;
	avail->refcnt = 0;
	PSC_List_append(entries, avail, deleteEntry);
    }
    avail->color = self->color;
    ++avail->refcnt;
    self->entry = avail;

    xcb_connection_t *c = X11Adapter_connection();
    if (!avail->picture)
    {
	xcb_pixmap_t tmp = xcb_generate_id(c);
	static const uint8_t depths[] = {8, 24, 32};
	CHECK(xcb_create_pixmap(c, depths[self->format], tmp,
		    X11Adapter_screen()->root, 1, 1),
		"Cannot create pen picture for 0x%x", (unsigned)ownerpic);
	avail->picture = xcb_generate_id(c);
	uint32_t repeat = XCB_RENDER_REPEAT_NORMAL;
	CHECK(xcb_render_create_picture(c, avail->picture, tmp,
		    X11Adapter_format(self->format),
		    XCB_RENDER_CP_REPEAT, &repeat),
		"Cannot create pen for 0x%x", (unsigned)ownerpic);
	xcb_free_pixmap(c, tmp);
    }
    static const xcb_rectangle_t rect = {0, 0, 1, 1};
    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		avail->picture, Color_xcb(self->color), 1, &rect),
	    "Cannot colorize pen for 0x%x", (unsigned)ownerpic);

    return self->entry->picture;
}

void Pen_destroy(Pen *self)
{
    if (!self) return;
    if (!--refcnt)
    {
	PSC_List_destroy(entries);
	entries = 0;
    }
    free(self);
}


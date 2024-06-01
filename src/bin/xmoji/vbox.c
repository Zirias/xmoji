#include "vbox.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);

static MetaVBox mo = MetaVBox_init("VBox",
	destroy, draw, 0, 0, minSize, 0);

#define dummy ((void *)&mo)

typedef struct VBoxItem
{
    void *widget;
    Size minSize;
} VBoxItem;

struct VBox
{
    Object base;
    PSC_List *items;
    Size minSize;
};

static void destroy(void *obj)
{
    VBox *self = obj;
    PSC_List_destroy(self->items);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    VBox *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return 0;
    int rc = 0;

    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	rc = Widget_draw(item->widget);
	if (rc < 0) break;
    }
    PSC_ListIterator_destroy(i);

    return rc;
}

static Size minSize(const void *obj)
{
    const VBox *self = Object_instance(obj);
    return self->minSize;
}

VBox *VBox_createBase(void *derived, void *parent)
{
    REGTYPE(0);

    VBox *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.base = Widget_createBase(derived, parent, IE_NONE);
    self->base.type = OBJTYPE;
    self->items = PSC_List_create();
    self->minSize = (Size){0, 0};

    return self;
}

void layout(VBox *self)
{
    PSC_ListIterator *i = PSC_List_iterator(self->items);

    Size contentSize = { 0, 0 };
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	contentSize.height += item->minSize.height;
	if (item->minSize.width > contentSize.width)
	{
	    contentSize.width = item->minSize.width;
	}
    }

    Box padding = Widget_padding(self);
    Pos contentOrigin = Widget_contentOrigin(self, contentSize);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Widget_setOrigin(item->widget, contentOrigin);
	Widget_setSize(item->widget, (Size){
		contentSize.width, item->minSize.height});
	contentOrigin.y += item->minSize.height;
    }

    PSC_ListIterator_destroy(i);

    self->minSize = (Size){
	contentSize.width + padding.left + padding.right,
	contentSize.height + padding.top + padding.bottom
    };
    Widget_requestSize(self);
}

void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)args;

    VBox *self = receiver;
    VBoxItem *item = 0;

    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *ci = PSC_ListIterator_current(i);
	if (ci->widget == sender)
	{
	    item = ci;
	    break;
	}
    }
    PSC_ListIterator_destroy(i);

    if (item)
    {
	Size minSize = Widget_minSize(item->widget);
	if (memcmp(&minSize, &item->minSize, sizeof minSize))
	{
	    item->minSize = minSize;
	    layout(self);
	}
    }
}

void VBox_addWidget(void *self, void *widget)
{
    VBox *b = Object_instance(self);
    VBoxItem *item = PSC_malloc(sizeof *item);
    item->widget = widget;
    item->minSize = (Size){0, 0};
    Widget_setDrawable(widget, Widget_drawable(b));
    PSC_Event_register(Widget_sizeRequested(widget), b, sizeRequested, 0);
    PSC_List_append(b->items, item, free);
    sizeRequested(b, widget, 0);
}


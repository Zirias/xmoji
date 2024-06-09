#include "vbox.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void *childAt(void *obj, Pos pos);
static void clicked(void *obj, const ClickEvent *event);

static MetaVBox mo = MetaVBox_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, childAt,
	minSize, 0, clicked, 0,
	"VBox", destroy);

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

static void expose(void *obj, Rect region)
{
    VBox *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Widget_invalidateRegion(item->widget, region);
    }
    PSC_ListIterator_destroy(i);
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

static void leave(void *obj)
{
    VBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Widget_leave(item->widget);
    }
    PSC_ListIterator_destroy(i);
}

static void *childAt(void *obj, Pos pos)
{
    VBox *self = Object_instance(obj);
    void *child = obj;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Rect rect = Widget_geometry(item->widget);
	if (pos.x >= rect.pos.x && pos.y >= rect.pos.y
		&& pos.x < rect.pos.x + rect.size.width
		&& pos.y < rect.pos.y + rect.size.height)
	{
	    child = Widget_enterAt(item->widget, pos);
	}
	else
	{
	    Widget_leave(item->widget);
	}
    }
    PSC_ListIterator_destroy(i);
    return child;
}

static void clicked(void *obj, const ClickEvent *event)
{
    const VBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Rect rect = Widget_geometry(item->widget);
	if (event->pos.x >= rect.pos.x
		&& event->pos.x < rect.pos.x + rect.size.width
		&& event->pos.y >= rect.pos.y
		&& event->pos.y < rect.pos.y + rect.size.height)
	{
	    Widget_clicked(item->widget, event);
	}
    }
    PSC_ListIterator_destroy(i);
}

void layout(VBox *self, int updateMinSize)
{
    PSC_ListIterator *i = PSC_List_iterator(self->items);

    if (updateMinSize)
    {
	self->minSize = (Size){ 0, 0 };
	while (PSC_ListIterator_moveNext(i))
	{
	    VBoxItem *item = PSC_ListIterator_current(i);
	    self->minSize.height += item->minSize.height;
	    if (item->minSize.width > self->minSize.width)
	    {
		self->minSize.width = item->minSize.width;
	    }
	}
	Widget_requestSize(self);
    }

    Size sz = Widget_size(self);
    Box padding = Widget_padding(self);
    uint16_t itemwidth = sz.width - padding.left - padding.right;
    uint16_t contentheight = sz.height - padding.top - padding.bottom;
    uint16_t hspace = 0;
    if (contentheight > self->minSize.height)
    {
	hspace = (contentheight - self->minSize.height)
	    / PSC_List_size(self->items);
    }
    Pos contentOrigin = Widget_contentOrigin(self, self->minSize);
    while (PSC_ListIterator_moveNext(i))
    {
	VBoxItem *item = PSC_ListIterator_current(i);
	Size itemSize = (Size){itemwidth, item->minSize.height + hspace};
	Widget_setSize(item->widget, itemSize);
	Size realSize = Widget_size(item->widget);
	Pos itemPos = contentOrigin;
	itemPos.x += (itemSize.width - realSize.width) / 2;
	itemPos.y += (itemSize.height - realSize.height) / 2;
	Widget_setOrigin(item->widget, itemPos);
	contentOrigin.y += itemSize.height;
    }

    PSC_ListIterator_destroy(i);
}

void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    VBox *self = receiver;
    layout(self, 0);
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
	    layout(self, 1);
	}
    }
}

VBox *VBox_createBase(void *derived, void *parent)
{
    REGTYPE(0);

    VBox *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.type = OBJTYPE;
    self->base.base = Widget_createBase(derived, 0, parent);
    self->items = PSC_List_create();
    self->minSize = (Size){0, 0};

    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

void VBox_addWidget(void *self, void *widget)
{
    VBox *b = Object_instance(self);
    VBoxItem *item = PSC_malloc(sizeof *item);
    item->widget = widget;
    item->minSize = (Size){0, 0};
    Widget_setContainer(widget, b);
    PSC_Event_register(Widget_sizeRequested(widget), b, sizeRequested, 0);
    PSC_List_append(b->items, item, free);
    sizeRequested(b, widget, 0);
}


#include "hbox.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaHBox mo = MetaHBox_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, 0,
	"HBox", destroy);

typedef struct HBoxItem
{
    Widget *widget;
    Size minSize;
    uint16_t minWidth;
} HBoxItem;

struct HBox
{
    Object base;
    PSC_List *items;
    Size minSize;
    uint16_t spacing;
};

static void destroy(void *obj)
{
    HBox *self = obj;
    PSC_List_destroy(self->items);
    free(self);
}

static void destroyItem(void *obj)
{
    HBoxItem *self = obj;
    Object_destroy(self->widget);
    free(self);
}

static void expose(void *obj, Rect region)
{
    HBox *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	Widget_invalidateRegion(item->widget, region);
    }
    PSC_ListIterator_destroy(i);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    HBox *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return 0;
    int rc = 0;

    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	rc = Widget_draw(item->widget);
	if (rc < 0) break;
    }
    PSC_ListIterator_destroy(i);

    return rc;
}

static Size minSize(const void *obj)
{
    const HBox *self = Object_instance(obj);
    return self->minSize;
}

static void leave(void *obj)
{
    HBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	Widget_leave(item->widget);
    }
    PSC_ListIterator_destroy(i);
}

static void unselect(void *obj)
{
    HBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	Widget_unselect(item->widget);
    }
    PSC_ListIterator_destroy(i);
}

static void setFont(void *obj, Font *font)
{
    HBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	Widget_offerFont(item->widget, font);
    }
    PSC_ListIterator_destroy(i);
}

static Widget *childAt(void *obj, Pos pos)
{
    HBox *self = Object_instance(obj);
    Widget *child = Widget_cast(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
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

static int clicked(void *obj, const ClickEvent *event)
{
    const HBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    int handled = 0;
    while (!handled && PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	Rect rect = Widget_geometry(item->widget);
	if (event->pos.x >= rect.pos.x
		&& event->pos.x < rect.pos.x + rect.size.width
		&& event->pos.y >= rect.pos.y
		&& event->pos.y < rect.pos.y + rect.size.height)
	{
	    handled = Widget_clicked(item->widget, event);
	}
    }
    PSC_ListIterator_destroy(i);
    return handled;
}

static void layout(HBox *self, int updateMinSize)
{
    PSC_ListIterator *i = PSC_List_iterator(self->items);

    if (updateMinSize)
    {
	self->minSize = (Size){ 0, 0 };
	while (PSC_ListIterator_moveNext(i))
	{
	    HBoxItem *item = PSC_ListIterator_current(i);
	    uint16_t minw = item->minSize.width > item->minWidth
		? item->minSize.width : item->minWidth;
	    self->minSize.width += minw + self->spacing;
	    if (item->minSize.height > self->minSize.height)
	    {
		self->minSize.height = item->minSize.height;
	    }
	}
	if (self->minSize.width) self->minSize.width -= self->spacing;
	Widget_requestSize(self);
    }

    Size sz = Widget_size(self);
    Box padding = Widget_padding(self);
    uint16_t itemheight = sz.height - padding.top - padding.bottom;
    uint16_t contentwidth = sz.width - padding.left - padding.right;
    uint16_t vspace = 0;
    if (contentwidth > self->minSize.width)
    {
	vspace = (contentwidth - self->minSize.width)
	    / PSC_List_size(self->items);
    }
    Pos contentOrigin = Widget_contentOrigin(self, self->minSize);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *item = PSC_ListIterator_current(i);
	uint16_t minw = item->minSize.width > item->minWidth
	    ? item->minSize.width : item->minWidth;
	Size itemSize = (Size){minw + vspace, itemheight};
	Widget_setSize(item->widget, itemSize);
	Size realSize = Widget_size(item->widget);
	Pos itemPos = contentOrigin;
	Align itemAlign = Widget_align(item->widget);
	if (itemAlign & AH_CENTER)
	{
	    itemPos.x += (itemSize.width - realSize.width) / 2;
	}
	else if (itemAlign & AH_RIGHT)
	{
	    itemPos.x += itemSize.width - realSize.width;
	}
	if (itemAlign & AV_MIDDLE)
	{
	    itemPos.y += (itemSize.height - realSize.height) / 2;
	}
	else if (itemAlign & AV_BOTTOM)
	{
	    itemPos.y += itemSize.height - realSize.height;
	}
	Widget_setOrigin(item->widget, itemPos);
	contentOrigin.x += itemSize.width + self->spacing;
    }

    PSC_ListIterator_destroy(i);
}

static void layoutChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    HBox *self = receiver;
    layout(self, 0);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)args;

    HBox *self = receiver;
    Widget *widget = sender;
    HBoxItem *item = 0;

    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	HBoxItem *ci = PSC_ListIterator_current(i);
	if (ci->widget == widget)
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

HBox *HBox_createBase(void *derived, void *parent)
{
    HBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, 0, parent);
    self->items = PSC_List_create();
    self->minSize = (Size){0, 0};
    self->spacing = 3;

    PSC_Event_register(Widget_sizeChanged(self), self, layoutChanged, 0);
    PSC_Event_register(Widget_originChanged(self), self, layoutChanged, 0);

    return self;
}

void HBox_addWidget(void *self, void *widget)
{
    HBox *b = Object_instance(self);
    HBoxItem *item = PSC_malloc(sizeof *item);
    item->widget = Widget_cast(Object_ref(widget));
    item->minSize = Widget_minSize(widget);
    item->minWidth = 0;
    Widget_setContainer(widget, b);
    PSC_Event_register(Widget_sizeRequested(widget), b, sizeRequested, 0);
    PSC_List_append(b->items, item, destroyItem);
    layout(b, 1);
}

uint16_t HBox_spacing(const void *self)
{
    HBox *b = Object_instance(self);
    return b->spacing;
}

void HBox_setSpacing(void *self, uint16_t spacing)
{
    HBox *b = Object_instance(self);
    if (spacing != b->spacing)
    {
	b->spacing = spacing;
	if (PSC_List_size(b->items)) layout(b, 1);
    }
}

unsigned HBox_cols(const void *self)
{
    const HBox *b = Object_instance(self);
    return PSC_List_size(b->items);
}

uint16_t HBox_minWidth(const void *self, unsigned col)
{
    const HBox *b = Object_instance(self);
    const HBoxItem *item = PSC_List_at(b->items, col);
    if (!item) return 0;
    return item->minWidth > item->minSize.width
	? item->minWidth : item->minSize.width;
}

void HBox_setMinWidth(void *self, unsigned col, uint16_t minWidth)
{
    HBox *b = Object_instance(self);
    HBoxItem *item = PSC_List_at(b->items, col);
    if (item)
    {
	item->minWidth = minWidth;
	layout(b, 1);
    }
}


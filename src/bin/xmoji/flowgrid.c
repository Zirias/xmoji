#include "flowgrid.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static int show(void *obj);
static int hide(void *obj);
static Size minSize(const void *obj);
static void leave(void *obj);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaFlowGrid mo = MetaFlowGrid_init(
	expose, draw, show, hide,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, 0,
	"FlowGrid", destroy);

typedef struct FlowGridItem
{
    FlowGrid *grid;
    void *widget;
    Size minSize;
} FlowGridItem;

struct FlowGrid
{
    Object base;
    PSC_List *items;
    Widget *hoverWidget;
    Size itemMinSize;
    Size minSize;
    Size spacing;
    int shown;
    uint16_t cols;
    uint16_t minCols;
};

static void destroy(void *obj)
{
    FlowGrid *self = obj;
    PSC_List_destroy(self->items);
    free(self);
}

static void expose(void *obj, Rect region)
{
    FlowGrid *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	Widget_invalidateRegion(item->widget, region);
    }
    PSC_ListIterator_destroy(i);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    FlowGrid *self = Object_instance(obj);
    if (!PSC_List_size(self->items)) return 0;
    int rc = 0;

    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	rc = Widget_draw(item->widget);
	if (rc < 0) break;
    }
    PSC_ListIterator_destroy(i);

    return rc;
}

static void layout(FlowGrid *self, int updateMinSize)
{
    if (!self->shown) return;
    PSC_ListIterator *i = PSC_List_iterator(self->items);

    if (updateMinSize)
    {
	self->itemMinSize = (Size){ 0, 0 };
	while (PSC_ListIterator_moveNext(i))
	{
	    FlowGridItem *item = PSC_ListIterator_current(i);
	    if (item->minSize.height > self->itemMinSize.height)
	    {
		self->itemMinSize.height = item->minSize.height;
	    }
	    if (item->minSize.width > self->itemMinSize.width)
	    {
		self->itemMinSize.width = item->minSize.width;
	    }
	}
    }
    if (self->itemMinSize.width == 0 || self->itemMinSize.height == 0)
    {
	goto done;
    }

    Rect geom = Widget_geometry(self);
    Box padding = Widget_padding(self);
    Rect contentGeom = Rect_pad(geom, padding);
    if (contentGeom.size.width < self->itemMinSize.width)
    {
	contentGeom.size.width = self->itemMinSize.width;
    }
    uint16_t cols = (contentGeom.size.width + self->spacing.width)
	/ (self->itemMinSize.width + self->spacing.width);
    if (cols < self->minCols) cols = self->minCols;
    Pos rowOrigin = contentGeom.pos;
    Pos colOrigin = rowOrigin;
    uint16_t col = 0;
    uint16_t rows = 0;
    while (PSC_ListIterator_moveNext(i))
    {
	if (!rows) rows = 1;
	FlowGridItem *item = PSC_ListIterator_current(i);
	if (!Widget_isShown(item->widget)) continue;
	Widget_setSize(item->widget, self->itemMinSize);
	Widget_setOrigin(item->widget, colOrigin);
	if (++col == cols)
	{
	    col = 0;
	    ++rows;
	    rowOrigin.y += self->itemMinSize.height + self->spacing.height;
	    colOrigin = rowOrigin;
	}
	else
	{
	    colOrigin.x += self->itemMinSize.width + self->spacing.width;
	}
    }
    Size minSz = (Size){self->minCols * self->itemMinSize.width
	+ (self->minCols - 1) * self->spacing.width,
	rows * self->itemMinSize.height + (rows-1) * self->spacing.height};
    if (memcmp(&self->minSize, &minSz, sizeof self->minSize))
    {
	self->minSize = minSz;
	Widget_requestSize(self);
    }

done:
    PSC_ListIterator_destroy(i);
}

static int show(void *obj)
{
    FlowGrid *self = Object_instance(obj);
    self->shown = 1;
    layout(self, 1);
    int rc = 1;
    Object_bcall(rc, Widget, show, obj);
    return rc;
}

static int hide(void *obj)
{
    FlowGrid *self = Object_instance(obj);
    self->shown = 0;
    int rc = 1;
    Object_bcall(rc, Widget, show, obj);
    return rc;
}

static Size minSize(const void *obj)
{
    const FlowGrid *self = Object_instance(obj);
    return self->minSize;
}

static void leave(void *obj)
{
    FlowGrid *self = Object_instance(obj);
    if (self->hoverWidget)
    {
	Widget_leave(self->hoverWidget);
	self->hoverWidget = 0;
    }
}

static void unselect(void *obj)
{
    FlowGrid *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	Widget_unselect(item->widget);
    }
    PSC_ListIterator_destroy(i);
}

static void setFont(void *obj, Font *font)
{
    FlowGrid *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	Widget_offerFont(item->widget, font);
    }
    PSC_ListIterator_destroy(i);
}

static Widget *childAt(void *obj, Pos pos)
{
    FlowGrid *self = Object_instance(obj);
    Widget *child = 0;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	if (!Widget_isShown(item->widget)) continue;
	Rect rect = Widget_geometry(item->widget);
	if (Rect_containsPos(rect, pos))
	{
	    child = Widget_enterAt(item->widget, pos);
	}
    }
    PSC_ListIterator_destroy(i);
    if (child != self->hoverWidget)
    {
	if (self->hoverWidget) Widget_leave(self->hoverWidget);
	self->hoverWidget = child;
    }
    return child ? child : Widget_cast(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    const FlowGrid *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    int handled = 0;
    while (!handled && PSC_ListIterator_moveNext(i))
    {
	FlowGridItem *item = PSC_ListIterator_current(i);
	Rect rect = Widget_geometry(item->widget);
	if (Rect_containsPos(rect, event->pos))
	{
	    handled = Widget_clicked(item->widget, event);
	}
    }
    PSC_ListIterator_destroy(i);
    return handled;
}

static void layoutChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    layout(receiver, 0);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    FlowGridItem *item = receiver;
    Size minSize = Widget_minSize(item->widget);
    if (memcmp(&minSize, &item->minSize, sizeof minSize))
    {
	item->minSize = minSize;
	layout(item->grid, 1);
    }
}

static void shownChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    FlowGrid *self = receiver;
    layout(self, 0);
    Widget_requestSize(self);
    Widget_invalidate(self);
}

static void destroyItem(void *obj)
{
    if (!obj) return;
    FlowGridItem *item = obj;
    PSC_Event_unregister(Widget_sizeRequested(item->widget), item,
	    sizeRequested, 0);
    PSC_Event_unregister(Widget_shown(item->widget), item->grid,
	    shownChanged, 0);
    PSC_Event_unregister(Widget_hidden(item->widget), item->grid,
	    shownChanged, 0);
    Object_destroy(item->widget);
    free(item);
}

FlowGrid *FlowGrid_createBase(void *derived, void *parent)
{
    FlowGrid *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    CREATEBASE(Widget, 0, parent);
    self->items = PSC_List_create();
    self->spacing = (Size){3, 3};
    self->minCols = 6;

    PSC_Event_register(Widget_sizeChanged(self), self, layoutChanged, 0);
    PSC_Event_register(Widget_originChanged(self), self, layoutChanged, 0);

    return self;
}

void FlowGrid_addWidget(void *self, void *widget)
{
    FlowGrid *g = Object_instance(self);
    FlowGridItem *item = PSC_malloc(sizeof *item);
    item->grid = g;
    item->widget = Object_ref(Widget_cast(widget));
    Widget_setContainer(widget, g);
    PSC_List_append(g->items, item, destroyItem);
    Font *font = Widget_font(g);
    if (font) Widget_offerFont(widget, font);
    item->minSize = Widget_minSize(widget);
    PSC_Event_register(Widget_sizeRequested(widget), item, sizeRequested, 0);
    PSC_Event_register(Widget_shown(widget), g, shownChanged, 0);
    PSC_Event_register(Widget_hidden(widget), g, shownChanged, 0);
    layout(g, 1);
}

void *FlowGrid_widgetAt(void *self, size_t index)
{
    FlowGrid *g = Object_instance(self);
    FlowGridItem *item = PSC_List_at(g->items, index);
    return item ? item->widget : 0;
}

Size FlowGrid_spacing(const void *self)
{
    FlowGrid *g = Object_instance(self);
    return g->spacing;
}

void FlowGrid_setSpacing(void *self, Size spacing)
{
    FlowGrid *g = Object_instance(self);
    if (memcmp(&spacing, &g->spacing, sizeof spacing))
    {
	g->spacing = spacing;
	if (PSC_List_size(g->items)) layout(g, 0);
    }
}


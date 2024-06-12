#include "scrollbox.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void unselect(void *obj);
static void *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaScrollBox mo = MetaScrollBox_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, unselect, childAt,
	minSize, 0, clicked, 0,
	"ScrollBox", destroy);

struct ScrollBox
{
    Object base;
    Widget *widget;
    Size minSize;
    Size scrollSize;
    Rect scrollBar;
};

static void updateScrollbar(ScrollBox *self, Size size)
{
    if (self->scrollSize.height <= size.height) return;
    uint32_t barHeight = ((size.height + 2) << 6) * (size.height << 6)
	/ (self->scrollSize.height << 6);
    self->scrollBar.size.height = (barHeight + 0x20) >> 6;
    if (self->scrollBar.size.height < 16) self->scrollBar.size.height = 16;
}

static void destroy(void *obj)
{
    ScrollBox *self = obj;
    free(self);
}

static void expose(void *obj, Rect region)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return;
    Widget_invalidateRegion(self->widget, region);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return 0;
    Size size = Widget_size(self);
    if (picture && self->scrollSize.height > size.height)
    {
	xcb_connection_t *c = X11Adapter_connection();
	Pos origin = Widget_origin(self);
	Color bgcol = Widget_color(self, COLOR_BG_BELOW);
	xcb_rectangle_t rect = {
	    origin.x + size.width - self->scrollBar.size.width - 2, origin.y,
	    self->scrollBar.size.width + 2, size.height };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER, picture,
		    Color_xcb(bgcol), 1, &rect),
		"Cannot draw scrollbar background on 0x%x", (unsigned)picture);
	Color barcol = Widget_color(self, COLOR_BG_ABOVE);
	rect = (xcb_rectangle_t){
	    origin.x + size.width - self->scrollBar.size.width - 1,
	    origin.y + 1,
	    self->scrollBar.size.width,
	    self->scrollBar.size.height
	};
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER, picture,
		    Color_xcb(barcol), 1, &rect),
		"Cannot draw scrollbar on 0x%x", (unsigned)picture);
    }
    return Widget_draw(self->widget);
}

static Size minSize(const void *obj)
{
    const ScrollBox *self = Object_instance(obj);
    return self->minSize;
}

static void leave(void *obj)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return;
    Widget_leave(self->widget);
}

static void unselect(void *obj)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return;
    Widget_unselect(self->widget);
}

static void *childAt(void *obj, Pos pos)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return self;
    return Widget_enterAt(self->widget, pos);
}

static int clicked(void *obj, const ClickEvent *event)
{
    const ScrollBox *self = Object_instance(obj);
    if (!self->widget) return 0;
    return Widget_clicked(self->widget, event);
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    SizeChangedEventArgs *ea = args;
    ScrollBox *self = receiver;
    if (!self->widget) return;
    Size sz = ea->newSize;
    if (self->scrollSize.height > sz.height)
    {
	updateScrollbar(self, sz);
	sz.width -= self->scrollBar.size.width + 2;
    }
    Widget_setSize(self->widget, sz);
    Widget_invalidate(self);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)args;

    ScrollBox *self = receiver;
    self->scrollSize = Widget_minSize(sender);
    self->minSize.width = self->scrollSize.width
	+ self->scrollBar.size.width + 2;
    Widget_requestSize(self);
}

ScrollBox *ScrollBox_createBase(void *derived, void *parent)
{
    REGTYPE(0);

    ScrollBox *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.type = OBJTYPE;
    self->base.base = Widget_createBase(derived, 0, parent);
    self->widget = 0;
    self->minSize = (Size){0, 100};
    self->scrollSize = (Size){0, 0};
    self->scrollBar = (Rect){{0, 0}, {10, 0}};

    Widget_setPadding(self, (Box){0, 0, 0, 0});
    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

void ScrollBox_setWidget(void *self, void *widget)
{
    ScrollBox *b = Object_instance(self);
    if (b->widget)
    {
	Widget_setContainer(b->widget, 0);
	PSC_Event_unregister(Widget_sizeRequested(b->widget), b,
		sizeRequested, 0);
    }
    b->widget = widget;
    Widget_setContainer(widget, b);
    PSC_Event_register(Widget_sizeRequested(widget), b, sizeRequested, 0);
    sizeRequested(b, widget, 0);
}


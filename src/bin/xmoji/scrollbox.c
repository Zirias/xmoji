#include "scrollbox.h"

#include "surface.h"
#include "window.h"
#include "xrdb.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);
static void dragged(void *obj, const DragEvent *event);

static MetaScrollBox mo = MetaScrollBox_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, dragged,
	"ScrollBox", destroy);

struct ScrollBox
{
    Object base;
    Widget *widget;
    Size minSize;
    Size scrollSize;
    Rect scrollBar;
    int backingstore;
    int hoverBar;
    uint16_t scrollPos;
    int16_t dragAnchor;
    uint16_t minBarHeight;
};

static void updateScrollbar(ScrollBox *self, Size size)
{
    Rect geom = Widget_geometry(self);
    if (self->scrollSize.height <= size.height)
    {
	self->scrollPos = 0;
	goto done;
    }
    if (self->scrollSize.height < size.height + self->scrollPos)
    {
	self->scrollPos = self->scrollSize.height - size.height;
    }
    uint32_t barHeight = ((size.height - 2) << 6) * (size.height << 6)
	/ (self->scrollSize.height << 6);
    self->scrollBar.size.height = (barHeight + 0x20) >> 6;
    uint16_t hmin = self->minBarHeight;
    if (hmin > geom.size.height / 2) hmin = geom.size.height / 2;
    if (self->scrollBar.size.height < hmin) self->scrollBar.size.height = hmin;
    uint32_t scrollHeight = (self->scrollSize.height - size.height) << 6;
    uint32_t scrollTop =
	((uint64_t)(size.height - self->scrollBar.size.height - 2) << 6)
	* (uint64_t)(self->scrollPos << 6) / scrollHeight;
    self->scrollBar.pos.y = (scrollTop + 0x20) >> 6;
done:
    geom.pos.y -= self->scrollPos;
    if (self->backingstore)
    {
	Widget_setOffset(self->widget, geom.pos);
    }
    else
    {
	Widget_setOrigin(self->widget, geom.pos);
	Widget_setClip(self->widget, Widget_geometry(self));
    }
    Window *win = Window_fromWidget(self);
    if (win) Window_invalidateHover(win);
}

static Rect scrollBarGeom(ScrollBox *self)
{
    Pos origin = Widget_origin(self);
    Size childSize = Widget_size(self->widget);
    Rect barGeom = self->scrollBar;
    barGeom.pos.x += origin.x + childSize.width + 1;
    barGeom.pos.y += origin.y + 1;
    return barGeom;
}

static void updateHover(ScrollBox *self, Pos pos)
{
    if (self->dragAnchor >= 0) return;
    int hover;
    Rect barGeom = scrollBarGeom(self);
    if (pos.x >= barGeom.pos.x
	    && pos.x < barGeom.pos.x + barGeom.size.width
	    && pos.y >= barGeom.pos.y
	    && pos.y < barGeom.pos.y + barGeom.size.height)
    {
	hover = 1;
    }
    else
    {
	hover = 0;
    }
    if (self->hoverBar != hover)
    {
	self->hoverBar = hover;
	Widget_invalidateRegion(self, barGeom);
    }
}

static void destroy(void *obj)
{
    ScrollBox *self = obj;
    if (!self->backingstore) Object_destroy(self->widget);
    free(self);
}

static void expose(void *obj, Rect region)
{
    ScrollBox *self = Object_instance(obj);
    if (self->backingstore || !self->widget) return;
    Widget_invalidateRegion(self->widget, region);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return 0;
    Size size = Widget_size(self);
    Pos origin = Widget_origin(self);
    if (picture && self->scrollSize.height > size.height)
    {
	xcb_connection_t *c = X11Adapter_connection();
	Color bgcol = Widget_color(self, COLOR_BG_LOWEST);
	xcb_rectangle_t rect = {
	    origin.x + size.width - self->scrollBar.size.width - 2, origin.y,
	    self->scrollBar.size.width + 2, size.height };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER, picture,
		    Color_xcb(bgcol), 1, &rect),
		"Cannot draw scrollbar background on 0x%x", (unsigned)picture);
	Color barcol = self->hoverBar
	    ? Widget_color(self, COLOR_BG_ACTIVE)
	    : Widget_color(self, COLOR_BG_ABOVE);
	rect = (xcb_rectangle_t){
	    origin.x + size.width - self->scrollBar.size.width - 1,
	    origin.y + self->scrollBar.pos.y + 1,
	    self->scrollBar.size.width,
	    self->scrollBar.size.height
	};
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER, picture,
		    Color_xcb(barcol), 1, &rect),
		"Cannot draw scrollbar on 0x%x", (unsigned)picture);
    }
    int rc = Widget_draw(self->widget);
    if (picture && self->backingstore)
    {
	Pos offset = Widget_offset(self->widget);
	Size ssz = Widget_size(self->widget);
	Rect rect = {
	    { origin.x - offset.x, origin.y - offset.y },
	    { ssz.width, ssz.height }
	};
	Surface_render(self->widget, picture, origin, rect);
    }
    return rc;
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
    self->dragAnchor = -1;
    updateHover(self, (Pos){-1, -1});
    Widget_leave(self->widget);
}

static void unselect(void *obj)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return;
    Widget_unselect(self->widget);
}

static void setFont(void *obj, Font *font)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return;
    Widget_offerFont(self->widget, font);
}

static Widget *childAt(void *obj, Pos pos)
{
    ScrollBox *self = Object_instance(obj);
    Widget *child = Widget_cast(self);
    if (!self->widget) return child;
    Rect childGeom = Widget_geometry(self->widget);
    if (pos.x >= childGeom.pos.x
	    && pos.x < childGeom.pos.x + childGeom.size.width
	    && pos.y >= childGeom.pos.y
	    && pos.y < childGeom.pos.y + childGeom.size.height)
    {
	self->dragAnchor = -1;
	child = Widget_enterAt(self->widget, pos);
    }
    updateHover(self, pos);
    return child;
}

static int clicked(void *obj, const ClickEvent *event)
{
    ScrollBox *self = Object_instance(obj);
    if (!self->widget) return 0;
    self->dragAnchor = -1;
    Rect geom = Widget_geometry(self);
    if (event->pos.x < geom.pos.x + geom.size.width
	    - self->scrollBar.size.width - 2)
    {
	if (Widget_clicked(self->widget, event)) return 1;
    }
    if (event->button == MB_WHEEL_UP)
    {
	if (self->scrollPos < 24) self->scrollPos = 0;
	else self->scrollPos -= 24;
	updateScrollbar(self, geom.size);
	updateHover(self, event->pos);
	Widget_invalidate(self);
	return 1;
    }
    if (event->button == MB_WHEEL_DOWN)
    {
	self->scrollPos += 24;
	updateScrollbar(self, geom.size);
	updateHover(self, event->pos);
	Widget_invalidate(self);
	return 1;
    }
    if (event->button == MB_LEFT)
    {
	if (event->pos.x < geom.pos.x + geom.size.width
		- self->scrollBar.size.width - 2) return 0;
	Rect barGeom = scrollBarGeom(self);
	if (event->pos.y < barGeom.pos.y)
	{
	    if (self->scrollPos < 24) self->scrollPos = 0;
	    else self->scrollPos -= 24;
	    updateScrollbar(self, geom.size);
	    updateHover(self, event->pos);
	    Widget_invalidate(self);
	}
	else if (event->pos.y >= barGeom.pos.y + barGeom.size.height)
	{
	    self->scrollPos += 24;
	    updateScrollbar(self, geom.size);
	    updateHover(self, event->pos);
	    Widget_invalidate(self);
	}
	return 1;
    }
    return 0;
}

static void dragged(void *obj, const DragEvent *event)
{
    ScrollBox *self = Object_instance(obj);
    if (!event->button) self->dragAnchor = -1;
    if (!self->hoverBar || event->button != MB_LEFT) return;
    if (self->dragAnchor < 0) self->dragAnchor = self->scrollBar.pos.y;
    Rect geom = Widget_geometry(self);
    uint16_t ymax = geom.size.height - self->scrollBar.size.height - 2;
    int16_t yoff = event->to.y - event->from.y;
    int16_t ypos = self->dragAnchor + yoff;
    if (ypos < 0) ypos = 0;
    if (ypos > ymax) ypos = ymax;
    self->scrollBar.pos.y = ypos;
    uint32_t scrollHeight = (self->scrollSize.height - geom.size.height) << 6;
    uint32_t scrollPos = ((uint64_t)ypos << 6) * (uint64_t)scrollHeight
	/ (ymax << 6);
    self->scrollPos = (scrollPos + 0x20) >> 6;
    Pos origin = geom.pos;
    origin.y -= self->scrollPos;
    if (self->backingstore) Widget_setOffset(self->widget, origin);
    else Widget_setOrigin(self->widget, origin);
    Widget_invalidate(self);
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    SizeChangedEventArgs *ea = args;
    ScrollBox *self = receiver;
    if (!self->widget) return;
    Size sz = ea->newSize;
    updateScrollbar(self, sz);
    if (self->scrollSize.height > sz.height)
    {
	sz.width -= self->scrollBar.size.width + 2;
	sz.height = self->scrollSize.height;
    }
    Widget_setSize(self->widget, sz);
    Widget_invalidate(self);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)args;

    ScrollBox *self = receiver;
    Size scrollSize = Widget_minSize(sender);
    Size curSize = Widget_size(sender);
    if (scrollSize.height > curSize.height)
    {
	curSize.height = scrollSize.height;
	Widget_setSize(sender, curSize);
    }
    self->minSize.width = scrollSize.width + self->scrollBar.size.width + 2;
    Widget_requestSize(self);
    if (memcmp(&self->scrollSize, &scrollSize, sizeof self->scrollSize))
    {
	self->scrollSize = scrollSize;
	updateScrollbar(self, Widget_size(self));
    }
}

ScrollBox *ScrollBox_createBase(void *derived, const char *name, void *parent)
{
    ScrollBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    XRdb *rdb = X11Adapter_resources();
    const char *resname = Widget_resname(self);
    self->minSize = (Size){0, 100};
    self->scrollSize = (Size){0, 0};
    self->scrollBar = (Rect){{0, 0},
	{XRdb_int(rdb, XRdbKey(resname, "scrollBarWidth"), XRQF_OVERRIDES,
		10, 2, 128), 0}};
    self->hoverBar = 0;
    self->scrollPos = 0;
    self->dragAnchor = -1;
    self->minBarHeight = XRdb_int(rdb, XRdbKey(resname, "scrollBarMinHeight"),
	    XRQF_OVERRIDES, 16, 2, 256);

    if ((self->backingstore = XRdb_bool(rdb,
		    XRdbKey(resname, "backingStore"), XRQF_OVERRIDES, 1)))
    {
	self->widget = Widget_cast(Surface_create(self));
	Widget_setContainer(self->widget, self);
	PSC_Event_register(Widget_sizeRequested(self->widget), self,
		sizeRequested, 0);
	Widget_show(self->widget);
    }
    else self->widget = 0;

    Widget_setPadding(self, (Box){0, 0, 0, 0});
    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

void ScrollBox_setWidget(void *self, void *widget)
{
    ScrollBox *b = Object_instance(self);
    if (b->backingstore)
    {
	Surface_setWidget(b->widget, widget);
	return;
    }
    if (b->widget)
    {
	Widget_setContainer(b->widget, 0);
	PSC_Event_unregister(Widget_sizeRequested(b->widget), b,
		sizeRequested, 0);
	Object_destroy(b->widget);
    }
    if (widget)
    {
	b->widget = Object_ref(widget);
	Widget_setContainer(widget, b);
	PSC_Event_register(Widget_sizeRequested(widget), b, sizeRequested, 0);
	sizeRequested(b, widget, 0);
    }
    else b->widget = 0;
}


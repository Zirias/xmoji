#include "surface.h"

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

static MetaSurface mo = MetaSurface_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, 0,
	"Surface", destroy);

struct Surface
{
    Object base;
    Widget *widget;
    xcb_pixmap_t p;
    xcb_render_picture_t pic;
};

static void destroy(void *obj)
{
    Surface *self = obj;
    Object_destroy(self->widget);
    if (self->p)
    {
	xcb_connection_t *c = X11Adapter_connection();
	xcb_render_free_picture(c, self->pic);
	xcb_free_pixmap(c, self->p);
    }
    free(self);
}

static void expose(void *obj, Rect region)
{
    Surface *self = Object_instance(obj);
    if (self->widget) Widget_invalidateRegion(self->widget, region);
    Widget *container = Widget_container(self);
    if (container)
    {
	Pos offset = Widget_offset(self);
	region.pos.x += offset.x;
	region.pos.y += offset.y;
	Widget_invalidateRegion(container, region);
    }
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    Surface *self = Object_instance(obj);
    int rc = 0;
    if (self->widget) rc = Widget_draw(self->widget);
    return rc;
}

static Size minSize(const void *obj)
{
    const Surface *self = Object_instance(obj);
    return self->widget ? Widget_minSize(self->widget) : (Size){ 0, 0 };
}

static void leave(void *obj)
{
    Surface *self = Object_instance(obj);
    if (self->widget) Widget_leave(self->widget);
}

static void unselect(void *obj)
{
    Surface *self = Object_instance(obj);
    if (self->widget) Widget_unselect(self->widget);
}

static void setFont(void *obj, Font *font)
{
    Surface *self = Object_instance(obj);
    if (self->widget) Widget_offerFont(self->widget, font);
}

static Widget *childAt(void *obj, Pos pos)
{
    Surface *self = Object_instance(obj);
    return self->widget
	? Widget_enterAt(self->widget, pos) : Widget_cast(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    const Surface *self = Object_instance(obj);
    return self->widget ? Widget_clicked(self->widget, event) : 0;
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Surface *self = receiver;
    SizeChangedEventArgs *ea = args;

    if (ea->newSize.width > 8192 || ea->newSize.height > 8192) return;
    xcb_connection_t *c = 0;
    if (ea->newSize.width > ea->oldSize.width
	    || ea->newSize.height > ea->oldSize.height)
    {
	if (self->p)
	{
	    c = X11Adapter_connection();
	    xcb_render_free_picture(c, self->pic);
	    xcb_free_pixmap(c, self->p);
	}
	self->p = 0;
	self->pic = 0;
    }

    if (ea->newSize.width && ea->newSize.height && !self->p)
    {
	if (!c) c = X11Adapter_connection();
	self->p = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 24, self->p, X11Adapter_screen()->root,
		    ea->newSize.width, ea->newSize.height),
		"Cannot create backing store pixmap for 0x%x",
		(unsigned)Widget_picture(self));
	Widget_setDrawable(self, self->p);
	self->pic = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, self->pic, self->p,
		    X11Adapter_format(PICTFORMAT_RGB), 0, 0),
		"Cannot create backing store picture for 0x%x",
		(unsigned)Widget_picture(self));
    }

    if (self->widget) Widget_setSize(self->widget, ea->newSize);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Widget_requestSize(receiver);
}

Surface *Surface_createBase(void *derived, void *parent)
{
    Surface *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, 0, parent);
    self->widget = 0;
    self->p = 0;

    Widget_setPadding(self, (Box){ 0, 0, 0, 0 });
    Widget_setBackground(self, 1, COLOR_BG_NORMAL);
    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

void Surface_setWidget(void *self, void *widget)
{
    Surface *s = Object_instance(self);
    if (s->widget)
    {
	PSC_Event_unregister(Widget_sizeRequested(s->widget), s,
		sizeRequested, 0);
	Widget_setContainer(s->widget, 0);
	Object_destroy(s->widget);
    }
    if (widget)
    {
	s->widget = Object_ref(Widget_cast(widget));
	Widget_setContainer(s->widget, s);
	Font *font = Widget_font(s);
	if (font) Widget_offerFont(s->widget, font);
	PSC_Event_register(Widget_sizeRequested(s->widget), s,
		sizeRequested, 0);
	sizeRequested(s, 0, 0);
    }
    else s->widget = 0;
}

void Surface_render(void *self, xcb_render_picture_t picture,
	Pos pos, Rect rect)
{
    Surface *s = Object_instance(self);
    CHECK(xcb_render_composite(X11Adapter_connection(), XCB_RENDER_PICT_OP_SRC,
		s->pic, 0, picture, rect.pos.x, rect.pos.y, 0, 0,
		pos.x, pos.y, rect.size.width, rect.size.height),
	    "Cannot render surface to 0x%x", (unsigned)picture);
}

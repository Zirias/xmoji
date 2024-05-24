#include "window.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static int show(void *obj);
static int hide(void *obj);

static MetaWindow mo = MetaWindow_init("Window",
	destroy, draw, show, hide, 0);

struct Window
{
    Object base;
    PSC_Event *closed;
    PSC_Event *errored;
    char *title;
    char *iconName;
    void *mainWidget;
    xcb_window_t w;
    int haserror;
    int haveMinSize;
    int mapped;
    int wantmap;
};

static void map(Window *self)
{
    CHECK(xcb_map_window(X11Adapter_connection(), self->w),
	    "Cannot map window 0x%x", (unsigned)self->w);
    self->mapped = 1;
    PSC_Log_fmt(PSC_L_DEBUG, "Mapping window 0x%x", (unsigned)self->w);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    Window *self = Object_instance(obj);
    if (!self->mainWidget) return -1;
    return Widget_draw(self->mainWidget);
}

static int show(void *obj)
{
    Window *self = Object_instance(obj);
    if (self->mapped) return 0;
    if (self->haveMinSize) map(self);
    else self->wantmap = 1;
    return 0;
}

static int hide(void *obj)
{
    Window *self = Object_instance(obj);
    if (!self->mapped) return 0;
    self->wantmap = -1;
    self->mapped = 0;
    CHECK(xcb_unmap_window(X11Adapter_connection(), self->w),
	    "Cannot map window 0x%x", (unsigned)self->w);
    PSC_Log_fmt(PSC_L_DEBUG, "Unmapping window 0x%x", (unsigned)self->w);
    return 0;
}

static void expose(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_expose_event_t *ev = args;

    PSC_Log_fmt(PSC_L_DEBUG, "Expose 0x%x: { %u, %u, %u, %u }, count %u",
	    (unsigned)self->w, (unsigned)ev->x, (unsigned)ev->y,
	    (unsigned)ev->width, (unsigned)ev->height, (unsigned)ev->count);
    if (ev->count || !self->mapped) return;
    if (self->mapped == 1)
    {
	Widget_showWindow(receiver);
	self->mapped = 2;
    }
    Widget_invalidate(self);
}

static void configureNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_configure_notify_event_t *ev = args;
    Size oldsz = Widget_size(self);
    Size newsz = (Size){ ev->width, ev->height };

    if (memcmp(&oldsz, &newsz, sizeof oldsz))
    {
	Widget_setWindowSize(self, newsz);
    }
}

static void clientmsg(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_client_message_event_t *ev = args;
    
    if (ev->data.data32[0] == A(WM_DELETE_WINDOW))
    {
	Widget_disableDrawing(self);
	hide(self);
    }
}

static void requestError(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    if (self->haserror) return;
    PSC_Log_setAsync(0);
    PSC_Log_fmt(PSC_L_ERROR, "Window 0x%x failed", (unsigned)self->w);
    self->haserror = 1;
    PSC_Event_raise(self->errored, 0, 0);
}

static void trydraw(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Widget_draw(receiver);
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    SizeChangedEventArgs *ea = args;
    if (!ea->external)
    {
	uint16_t mask = 0;
	uint32_t values[2];
	int n = 0;
	if (ea->newSize.width != ea->oldSize.width)
	{
	    values[n++] = ea->newSize.width;
	    mask |= XCB_CONFIG_WINDOW_WIDTH;
	}
	if (ea->newSize.height != ea->oldSize.height)
	{
	    values[n++] = ea->newSize.height;
	    mask |= XCB_CONFIG_WINDOW_HEIGHT;
	}
	CHECK(xcb_configure_window(X11Adapter_connection(),
		    self->w, mask, values),
		"Cannot configure window 0x%x", (unsigned)self->w);
    }
    else if (self->mapped
	    && ea->newSize.width <= ea->oldSize.width
	    && ea->newSize.height <= ea->oldSize.height)
    {
	// Only if one dimension of the new size is larger than the old size,
	// we will get an expose event. Otherwise, invalidate our widget to
	// force a redraw.
	Widget_invalidate(self);
    }
    if (self->mainWidget) Widget_setSize(self->mainWidget, ea->newSize);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    Size minSize = Widget_minSize(self->mainWidget);
    if (minSize.width && minSize.height) self->haveMinSize = 1;
    else self->haveMinSize = 0;
    Size newSize = Widget_size(self);
    if (minSize.width > newSize.width) newSize.width = minSize.width;
    if (minSize.height > newSize.height) newSize.height = minSize.height;
    Widget_setSize(self, newSize);
    WMSizeHints hints = {
	.flags = WM_SIZE_HINT_P_MIN_SIZE,
	.min_width = minSize.width,
	.min_height = minSize.height
    };
    CHECK(xcb_change_property(X11Adapter_connection(), XCB_PROP_MODE_REPLACE,
		self->w, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS,
		32, sizeof hints >> 2, &hints),
	    "Cannot set minimum size on 0x%x", (unsigned)self->w);
    if (self->haveMinSize && !self->mapped && self->wantmap > 0)
    {
	self->wantmap = 0;
	map(self);
    }
}

static void mapped(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    self->mapped = 1;
}

static void unmapped(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    Widget_hideWindow(self);
    if (self->wantmap < 0)
    {
	self->wantmap = 0;
	PSC_Event_raise(self->closed, 0, 0);
    }
}

static void destroy(void *window)
{
    Window *self = window;
    PSC_Event_unregister(Widget_sizeChanged(self), self, sizeChanged, 0);
    PSC_Event_unregister(X11Adapter_eventsDone(), self, trydraw, 0);
    PSC_Event_unregister(X11Adapter_unmapNotify(), self,
	    unmapped, self->w);
    PSC_Event_unregister(X11Adapter_mapNotify(), self,
	    mapped, self->w);
    PSC_Event_unregister(X11Adapter_expose(), self,
	    expose, self->w);
    PSC_Event_unregister(X11Adapter_configureNotify(), self,
	    configureNotify, self->w);
    PSC_Event_unregister(X11Adapter_clientmsg(), self,
	    clientmsg, self->w);
    PSC_Event_unregister(X11Adapter_requestError(), self,
	    requestError, self->w);
    PSC_Event_destroy(self->closed);
    PSC_Event_destroy(self->errored);
    free(self->iconName);
    free(self->title);
    free(self);
}

Window *Window_createBase(void *derived, void *parent)
{
    REGTYPE(0);

    Window *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    memset(self, 0, sizeof *self);
    self->base.base = Widget_createBase(derived, parent);
    self->base.type = OBJTYPE;
    self->closed = PSC_Event_create(self);
    self->errored = PSC_Event_create(self);

    xcb_connection_t *c = X11Adapter_connection();
    self->w = xcb_generate_id(c);
    PSC_Event_register(X11Adapter_requestError(), self, requestError, self->w);

    xcb_screen_t *s = X11Adapter_screen();
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[] = { 
	XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };
    CHECK(xcb_create_window(c, XCB_COPY_FROM_PARENT, self->w, s->root,
		0, 0, 1, 1, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		s->root_visual, mask, values),
	    "Cannot create window 0x%x", (unsigned)self->w);
    xcb_atom_t delwin = A(WM_DELETE_WINDOW);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		A(WM_PROTOCOLS), 4, 32, 1, &delwin),
	    "Cannot set supported protocols on 0x%x", (unsigned)self->w);
    size_t sz;
    const char *wmclass = X11Adapter_wmClass(&sz);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sz, wmclass),
	    "Cannot set window class for 0x%x", (unsigned)self->w);
    xcb_atom_t wtnorm = A(_NET_WM_WINDOW_TYPE_NORMAL);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		A(_NET_WM_WINDOW_TYPE), 4, 32, 1, &wtnorm),
	    "Cannot set window type for 0x%x", (unsigned)self->w);

    Widget_setSize(self, (Size){1, 1});
    Widget_setDrawable(self, self->w);
    Widget_setBackground(self, 1, COLOR_BG_NORMAL);

    PSC_Event_register(X11Adapter_clientmsg(), self,
	    clientmsg, self->w);
    PSC_Event_register(X11Adapter_configureNotify(), self,
	    configureNotify, self->w);
    PSC_Event_register(X11Adapter_expose(), self,
	    expose, self->w);
    PSC_Event_register(X11Adapter_mapNotify(), self,
	    mapped, self->w);
    PSC_Event_register(X11Adapter_unmapNotify(), self,
	    unmapped, self->w);
    PSC_Event_register(X11Adapter_eventsDone(), self,
	    trydraw, 0);
    PSC_Event_register(Widget_sizeChanged(self), self,
	    sizeChanged, 0);

    return self;
}

PSC_Event *Window_closed(void *self)
{
    Window *w = Object_instance(self);
    return w->closed;
}

PSC_Event *Window_errored(void *self)
{
    Window *w = Object_instance(self);
    return w->errored;
}

const char *Window_title(const void *self)
{
    const Window *w = Object_instance(self);
    return w->title;
}

void Window_setTitle(void *self, const char *title)
{
    Window *w = Object_instance(self);
    if (!w->title && !title) return;
    if (w->title && title && !strcmp(w->title, title)) return;
    free(w->title);
    xcb_connection_t *c = X11Adapter_connection();
    if (title)
    {
	w->title = PSC_copystr(title);
	char *latintitle = X11Adapter_toLatin1(w->title);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, strlen(latintitle), latintitle);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w, A(_NET_WM_NAME),
		A(UTF8_STRING), 8, strlen(w->title), w->title);
	free(latintitle);
    }
    else
    {
	w->title = 0;
	xcb_delete_property(c, w->w, XCB_ATOM_WM_NAME);
	xcb_delete_property(c, w->w, A(_NET_WM_NAME));
    }
    if (!w->iconName) Window_setIconName(self, title);
}

const char *Window_iconName(const void *self)
{
    const Window *w = Object_instance(self);
    if (w->iconName) return w->iconName;
    return w->title;
}

void Window_setIconName(void *self, const char *iconName)
{
    Window *w = Object_instance(self);
    if (!w->iconName && !iconName) return;
    const char *newname = iconName;
    if (!newname) newname = w->title;
    if (w->iconName && newname && !strcmp(w->iconName, newname)) return;
    free(w->iconName);
    w->iconName = 0;
    xcb_connection_t *c = X11Adapter_connection();
    if (newname)
    {
	if (iconName) w->iconName = PSC_copystr(iconName);
	char *latinIconName = X11Adapter_toLatin1(newname);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8,
		strlen(latinIconName), latinIconName);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		A(_NET_WM_ICON_NAME), A(UTF8_STRING), 8, strlen(newname),
		newname);
	free(latinIconName);
    }
    else
    {
	xcb_delete_property(c, w->w, XCB_ATOM_WM_ICON_NAME);
	xcb_delete_property(c, w->w, A(_NET_WM_ICON_NAME));
    }
}

void *Window_mainWidget(const void *self)
{
    Window *w = Object_instance(self);
    return w->mainWidget;
}

void Window_setMainWidget(void *self, void *widget)
{
    Window *w = Object_instance(self);
    if (w->mainWidget)
    {
	PSC_Event_unregister(Widget_sizeRequested(w->mainWidget), w,
		sizeRequested, 0);
	Widget_setDrawable(w->mainWidget, 0);
    }
    w->mainWidget = widget;
    if (widget)
    {
	Widget_setDrawable(widget, w->w);
	PSC_Event_register(Widget_sizeRequested(widget), w, sizeRequested, 0);
	sizeRequested(w, 0, 0);
    }
}


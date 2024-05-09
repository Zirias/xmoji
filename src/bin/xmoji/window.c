#include "window.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static int show(void *window);
static int hide(void *window);

static MetaWindow mo = MetaWindow_init("Window", destroy, show, hide);

struct Window
{
    Object base;
    X11Adapter *x11;
    PSC_Event *closed;
    char *title;
    xcb_window_t w;
    uint32_t width;
    uint32_t height;
};

static void clientmsg(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_client_message_event_t *ev = args;
    X11Adapter *x11 = self->x11;
    
    if (ev->data.data32[0] == A(WM_DELETE_WINDOW))
    {
	PSC_Event_raise(self->closed, 0, 0);
    }
}

static void create_window_cb(void *ctx, void *reply,
	xcb_generic_error_t *error)
{
    (void)ctx;
    (void)reply;

    if (error)
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR, "Cannot create window");
	PSC_Service_quit();
    }
}

static void set_protocol_cb(void *ctx, void *reply,
	xcb_generic_error_t *error)
{
    (void)ctx;
    (void)reply;

    if (error)
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR,
		"Cannot set supported window manager protocols");
	PSC_Service_quit();
    }
}

static int show(void *window)
{
    Window *self = window;
    xcb_connection_t *c = X11Adapter_connection(self->x11);
    xcb_map_window(c, self->w);
    xcb_flush(c);
    return 0;
}

static int hide(void *window)
{
    (void)window;
    return 0;
}

static void destroy(void *window)
{
    Window *self = window;
    PSC_Event_unregister(X11Adapter_clientmsg(self->x11), self,
	    clientmsg, self->w);
    PSC_Event_destroy(self->closed);
    free(self->title);
    free(self);
}

Window *Window_create(X11Adapter *x11)
{
    if (!mo.base.id)
    {
	if (MetaObject_register(&mo) < 0) return 0;
    }

    xcb_connection_t *c = X11Adapter_connection(x11);
    xcb_screen_t *s = X11Adapter_screen(x11);
    xcb_window_t w = xcb_generate_id(c);
    if (!w) return 0;

    Window *self = PSC_malloc(sizeof *self);
    self->base.base = 0;
    self->base.type = mo.base.id;
    self->x11 = x11;
    self->closed = PSC_Event_create(self);
    self->title = 0;
    self->w = w;
    self->width = 320;
    self->height = 200;

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = { s->white_pixel, XCB_EVENT_MASK_STRUCTURE_NOTIFY };

    xcb_void_cookie_t cookie;
    cookie = xcb_create_window_checked(c, XCB_COPY_FROM_PARENT, w, s->root,
	    0, 0, self->width, self->height, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	    s->root_visual, mask, values);
    X11Adapter_await(x11, &cookie, self, create_window_cb);
    xcb_atom_t delwin = A(WM_DELETE_WINDOW);
    cookie = xcb_change_property_checked(c, XCB_PROP_MODE_REPLACE, w,
	    A(WM_PROTOCOLS), 4, 32, 1, &delwin);
    X11Adapter_await(x11, &cookie, self, set_protocol_cb);

    PSC_Event_register(X11Adapter_clientmsg(x11), self, clientmsg, w);

    return self;
}

PSC_Event *Window_closed(void *self)
{
    Window *w = Object_instance(self);
    return w->closed;
}

void Window_show(void *self)
{
    Object_vcallv(Window, show, self);
}

void Window_hide(void *self)
{
    Object_vcallv(Window, hide, self);
}

uint32_t Window_width(const void *self)
{
    const Window *w = Object_instance(self);
    return w->width;
}

uint32_t Window_height(const void *self)
{
    const Window *w = Object_instance(self);
    return w->height;
}

void Window_setSize(void *self, uint32_t width, uint32_t height)
{
    Window *w = Object_instance(self);
    uint16_t mask = 0;
    uint32_t values[2];
    int n = 0;
    if (width != w->width)
    {
	w->width = width;
	values[n++] = width;
	mask |= XCB_CONFIG_WINDOW_WIDTH;
    }
    if (height != w->height)
    {
	w->height = height;
	values[n++] = height;
	mask |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    if (n)
    {
	xcb_connection_t *c = X11Adapter_connection(w->x11);
	xcb_configure_window(c, w->w, mask, values);
	xcb_flush(c);
    }
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
    xcb_connection_t *c = X11Adapter_connection(w->x11);
    if (title)
    {
	w->title = PSC_copystr(title);
	xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, strlen(w->title), w->title);
    }
    else
    {
	w->title = 0;
	xcb_delete_property(c, w->w, XCB_ATOM_WM_NAME);
    }
    xcb_flush(c);
}


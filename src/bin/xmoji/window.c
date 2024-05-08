#include "window.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>

static void *create(void *options);
static void destroy(void *obj);
static int show(void *window);
static int hide(void *window);

static MetaWindow meta = {
    .base = {
	.id = 0,
	.baseId = 0,
	.name = "Window",
	.create = create,
	.destroy = destroy
    },
    .show = show,
    .hide = hide
};

struct Window
{
    Object base;
    X11Adapter *x11;
    PSC_Event *closed;
    xcb_window_t w;
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

static void *create(void *options)
{
    if (!meta.base.id)
    {
	if (MetaObject_register(&meta) < 0) return 0;
    }

    X11Adapter *x11 = options;
    xcb_connection_t *c = X11Adapter_connection(x11);
    xcb_screen_t *s = X11Adapter_screen(x11);
    xcb_window_t w = xcb_generate_id(c);
    if (!w) return 0;

    Window *self = PSC_malloc(sizeof *self);
    self->base.base = Object_create(0, 0);
    self->base.type = meta.base.id;
    self->x11 = x11;
    self->closed = PSC_Event_create(self);
    self->w = w;

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = { s->white_pixel, XCB_EVENT_MASK_STRUCTURE_NOTIFY };

    xcb_create_window(c, XCB_COPY_FROM_PARENT, w, s->root, 0, 0, 100, 100, 2,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT, s->root_visual, mask, values);
    xcb_atom_t delwin = A(WM_DELETE_WINDOW);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, A(WM_PROTOCOLS),
	    4, 32, 1, &delwin);

    PSC_Event_register(X11Adapter_clientmsg(x11), self, clientmsg, w);

    return self;
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
    free(self);
}

Window *Window_create(X11Adapter *x11)
{
    return create(x11);
}

PSC_Event *Window_closed(Window *self)
{
    return self->closed;
}

void Window_show(Window *self)
{
    MetaObject_callvoid(Window, show, self);
}

void Window_hide(Window *self)
{
    MetaObject_callvoid(Window, hide, self);
}


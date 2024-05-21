#include "window.h"

#include "font.h"
#include "textrenderer.h"
#include "xmoji.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static int draw(void *obj);
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
    xcb_window_t w;
    xcb_render_picture_t p;
    xcb_render_color_t bgcol;
    int haserror;
    Font *font[7];
    TextRenderer *hello[7];
};

static int draw(void *obj)
{
    Window *self = obj;
    xcb_rectangle_t rect = {0, 0, 0, 0};
    Size size = Widget_size(self);
    rect.width = size.width;
    rect.height = size.height;
    CHECK(xcb_render_fill_rectangles(X11Adapter_connection(),
		XCB_RENDER_PICT_OP_OVER, self->p, self->bgcol, 1, &rect),
	    "Cannot draw window background on 0x%x", (unsigned)self->w);
    Pos pos = { 0, 0 };
    for (int i = 0; i < 7; ++i)
    {
	TextRenderer_render(self->hello[i], self->p, pos);
	pos.y += Font_linespace(self->font[i]);
    }
    return 0;
}

static void expose(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Widget_draw(receiver);
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
	Widget_setSize(self, newsz);
	Widget_draw(self);
    }
}

static void clientmsg(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_client_message_event_t *ev = args;
    
    if (ev->data.data32[0] == A(WM_DELETE_WINDOW))
    {
	PSC_Event_raise(self->closed, 0, 0);
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

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    SizeChangedEventArgs *ea = args;
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
    xcb_configure_window(X11Adapter_connection(), self->w, mask, values);
}

static int show(void *obj)
{
    Window *self = obj;
    CHECK(xcb_map_window(X11Adapter_connection(), self->w),
	    "Cannot map window 0x%x", (unsigned)self->w);
    return Widget_show(Object_base(self));
}

static int hide(void *obj)
{
    (void)obj;
    return 0;
}

static void destroy(void *window)
{
    Window *self = window;
    xcb_connection_t *c = X11Adapter_connection();
    for (int i = 0; i < 7; ++i)
    {
	TextRenderer_destroy(self->hello[i]);
	Font_destroy(self->font[i]);
    }
    xcb_render_free_picture(c, self->p);
    PSC_Event_unregister(Widget_sizeChanged(self), self, sizeChanged, 0);
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
    free(self->title);
    free(self);
}

Window *Window_create(void)
{
    REGTYPE(0);

    xcb_connection_t *c = X11Adapter_connection();
    if (!c) return 0;
    xcb_screen_t *s = X11Adapter_screen();
    xcb_window_t w = xcb_generate_id(c);
    if (!w) return 0;

    Window *self = PSC_malloc(sizeof *self);
    self->base.base = Widget_create(0);
    self->base.type = OBJTYPE;
    self->closed = PSC_Event_create(self);
    self->errored = PSC_Event_create(self);
    self->title = 0;
    self->w = w;
    self->bgcol.red = 0xffff;
    self->bgcol.green = 0xffff;
    self->bgcol.blue = 0xffff;
    self->bgcol.alpha = 0xffff;
    self->haserror = 0;
    Widget_setSize(self, (Size){320, 200});

    PSC_Event_register(X11Adapter_requestError(), self, requestError, w);

    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[] = { 
	XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    CHECK(xcb_create_window(c, XCB_COPY_FROM_PARENT, w, s->root,
		0, 0, 320, 200, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		s->root_visual, mask, values),
	    "Cannot create window 0x%x", (unsigned)w);
    xcb_atom_t delwin = A(WM_DELETE_WINDOW);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		A(WM_PROTOCOLS), 4, 32, 1, &delwin),
	    "Cannot set supported protocols on 0x%x", (unsigned)w);
    size_t sz;
    const char *wmclass = X11Adapter_wmClass(&sz);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sz, wmclass),
	    "Cannot set window class for 0x%x", (unsigned)w);
    xcb_atom_t wtnorm = A(_NET_WM_WINDOW_TYPE_NORMAL);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		A(_NET_WM_WINDOW_TYPE), 4, 32, 1, &wtnorm),
	    "Cannot set window type for 0x%x", (unsigned)w);
    /*
    self->p = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, self->p, w, X11Adapter_rootformat(),
		0, 0),
	    "Cannot create XRender picture for 0x%x", (unsigned)w);
    */
    self->p = TextRenderer_createPicture(self->w);

    PSC_Event_register(X11Adapter_clientmsg(), self, clientmsg, w);
    PSC_Event_register(X11Adapter_configureNotify(), self, configureNotify, w);
    PSC_Event_register(X11Adapter_expose(), self, expose, w);
    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    for (int i = 0; i < 7; ++i)
    {
	self->font[i] = Font_create(i, 0);
	self->hello[i] = TextRenderer_create(self->font[i]);
	TextRenderer_setUtf8(self->hello[i], "Hello, World!");
    }
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

void Window_setBackgroundColor(void *self, Color color)
{
    Window *w = Object_instance(self);
    Color_setXcb(color, &w->bgcol);
}

void Window_setDefaultColor(void *self, Color color)
{
    Window *w = Object_instance(self);
    for (int i = 0; i < 7; ++i)
    {
	TextRenderer_setColor(w->hello[i], color);
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
    }
}


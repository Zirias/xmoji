#include "window.h"

#include "font.h"
#include "unistr.h"
#include "x11adapter.h"
#include "x11app-int.h"
#include "xrdb.h"
#include "xselection.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-compose.h>

#define DBLCLICK_MS 300
#define MAXDAMAGES 16

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static int show(void *obj);
static int hide(void *obj);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static int clicked(void *obj, const ClickEvent *event);

static MetaWindow mo = MetaWindow_init(expose, draw, show, hide,
	0, 0, 0, 0, 0, 0, 0,
	unselect, setFont, 0, 0, 0, clicked, 0,
	"Window", destroy);

struct Window
{
    Object base;
    PSC_Event *closed;
    PSC_Event *propertyChanged;
    XSelection *primary;
    XSelection *clipboard;
    struct xkb_compose_state *kbcompose;
    char *title;
    char *iconName;
    void *mainWidget;
    void *focusWidget;
    void *hoverWidget;
    Window *tooltipWindow;
    WindowFlags flags;
    Pos absMouse;
    Pos mouse;
    Pos mouseUpdate;
    Pos anchorPos;
    Size newSize;
    Rect damages[MAXDAMAGES];
    MouseButton anchorButton;
    WindowState state;
    WindowState hideState;
    XCursor cursor;
    xcb_window_t w;
    xcb_pixmap_t p;
    xcb_render_picture_t src;
    xcb_render_picture_t dst;
    xcb_timestamp_t clicktime;
    uint32_t borderpixel;
    int haveMinSize;
    int mapped;
    int wantmap;
    int ndamages;
    int havewmstate;
    uint16_t tmpProperties;
};

static Window *getParent(Window *self)
{
    Widget *parentWidget = Widget_container(self);
    return parentWidget ? Window_fromWidget(parentWidget) : 0;
}

static void map(Window *self)
{
    xcb_connection_t *c = X11Adapter_connection();
    WindowFlags wtype = self->flags & WF_WINDOW_TYPE;
    if (wtype == WF_WINDOW_TOOLTIP || wtype == WF_WINDOW_MENU)
    {
	Window *parent = getParent(self);
	Size size = Widget_minSize(self->mainWidget);
	size.width += 2;
	size.height += 2;
	xcb_screen_t *s = X11Adapter_screen();
	int16_t x = parent->absMouse.x - (size.width / 2);
	int16_t y = parent->absMouse.y - 16;
	if (self->flags & WF_POS_PARENTWIDGET)
	{
	    Widget *pw = Widget_container(self);
	    Pos parentpos = Widget_origin(pw);
	    Size parentsize = Widget_size(pw);
	    Pos offset = Widget_offset(pw);
	    parentpos.x += offset.x;
	    parentpos.y += offset.y;
	    while ((pw = Widget_container(pw)))
	    {
		offset = Widget_offset(pw);
		parentpos.x += offset.x;
		parentpos.y += offset.y;
	    }
	    x = parentpos.x + (parent->absMouse.x - parent->mouseUpdate.x);
	    y = parentpos.y + (parent->absMouse.y - parent->mouseUpdate.y);
	    Size sz = Widget_size(self);
	    if (self->flags & WF_POS_INCBORDER)
	    {
		parentsize.width -= 2;
		parentsize.height -= 2;
	    }
	    else
	    {
		--x;
		--y;
	    }
	    if (sz.width < parentsize.width) sz.width = parentsize.width;
	    if (sz.height < parentsize.height) sz.height = parentsize.height;
	    Widget_setSize(self, sz);
	}
	if (x + size.width > s->width_in_pixels)
	{
	    x = s->width_in_pixels - size.width;
	}
	if (x < 0) x = 0;
	if (wtype == WF_WINDOW_TOOLTIP) y -= size.height;
	if (wtype == WF_WINDOW_MENU
		&& y + size.height > s->height_in_pixels)
	{
	    y = s->height_in_pixels - size.height;
	}
	if (y < 0)
	{
	    if (wtype == WF_WINDOW_TOOLTIP
		    && parent->absMouse.y + 16 < s->height_in_pixels
		    && parent->absMouse.y + 16 >= 0)
	    {
		y = parent->absMouse.y + 16;
	    }
	    else y = 0;
	}
	self->absMouse = parent->absMouse;
	self->mouse = (Pos){-1, -1};
	self->mouseUpdate.x = self->absMouse.x - x;
	self->mouseUpdate.y = self->absMouse.y - y;
	CHECK(xcb_configure_window(c, self->w, XCB_CONFIG_WINDOW_X |
		    XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_BORDER_WIDTH |
		    XCB_CONFIG_WINDOW_STACK_MODE,
		    (uint32_t[]){x, y, 1, XCB_STACK_MODE_ABOVE}),
		"Cannot configure window 0x%x", (unsigned)self->w);
    }
    CHECK(xcb_map_window(c, self->w),
	    "Cannot map window 0x%x", (unsigned)self->w);
    self->mapped = 1;
    PSC_Log_fmt(PSC_L_DEBUG, "Mapping window 0x%x", (unsigned)self->w);
}

static void expose(void *obj, Rect region)
{
    Window *self = Object_instance(obj);
    if (self->mainWidget) Widget_invalidateRegion(self->mainWidget, region);
    if (self->p)
    {
	if (self->ndamages < 0) return;
	if (self->ndamages == MAXDAMAGES) self->ndamages = -1;
	else self->damages[self->ndamages++] = region;
    }
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    Window *self = Object_instance(obj);
    if (!self->mainWidget) return -1;
    int rc = Widget_draw(self->mainWidget);
    if (self->p)
    {
	if (self->ndamages < 0)
	{
	    Size size = Widget_size(self->mainWidget);
	    CHECK(xcb_render_composite(X11Adapter_connection(),
			XCB_RENDER_PICT_OP_SRC, self->src, 0, self->dst, 0, 0,
			0, 0, 0, 0, size.width, size.height),
		    "Cannot composite from backing store for 0x%x",
		    (unsigned)self->w);
	}
	else if (self->ndamages > 0) for (int i = 0; i < self->ndamages; ++i)
	{
	    CHECK(xcb_render_composite(X11Adapter_connection(),
			XCB_RENDER_PICT_OP_SRC, self->src, 0, self->dst,
			self->damages[i].pos.x, self->damages[i].pos.y, 0, 0,
			self->damages[i].pos.x, self->damages[i].pos.y,
			self->damages[i].size.width,
			self->damages[i].size.height),
		    "Cannot composite from backing store for 0x%x",
		    (unsigned)self->w);
	}
	self->ndamages = 0;
    }
    return rc;
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
    if (self->havewmstate && self->hideState == WS_MINIMIZED)
    {
	if (self->state == WS_MINIMIZED) return 0;
	xcb_client_message_event_t msg = {
	    .response_type = XCB_CLIENT_MESSAGE,
	    .format = 32,
	    .sequence = 0,
	    .window = self->w,
	    .type = A(WM_CHANGE_STATE),
	    .data = { .data32 = { WM_STATE_ICONIC, 0, 0, 0, 0 } }
	};
	CHECK(xcb_send_event(X11Adapter_connection(), 0,
		    X11Adapter_screen()->root,
		    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *)&msg),
		"Cannot request minimizing window 0x%x", (unsigned)self->w);
	PSC_Log_fmt(PSC_L_DEBUG, "Requesting to minimize window 0x%x",
		(unsigned)self->w);
    }
    else Window_close(self);
    if (self->hoverWidget)
    {
	Object_destroy(self->hoverWidget);
	self->hoverWidget = 0;
    }
    return 0;
}

static void unselect(void *obj)
{
    Window *self = Object_instance(obj);
    if (!self->mainWidget) return;
    Widget_unselect(self->mainWidget);
}

static void setFont(void *obj, Font *font)
{
    Window *self = Object_instance(obj);
    if (!self->mainWidget) return;
    Widget_offerFont(self->mainWidget, font);
}

static int clicked(void *obj, const ClickEvent *event)
{
    Window *self = Object_instance(obj);
    if (!self->mainWidget) return 0;
    return Widget_clicked(self->mainWidget, event);
}

static void buttonpress(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    if (!self->mainWidget) return;
    if (self->tooltipWindow && self->tooltipWindow->mapped)
    {
	Window_close(self->tooltipWindow);
    }
    xcb_button_press_event_t *ev = args;
    ClickEvent click = {
	.button = 1 << (ev->detail - 1),
	.pos = { ev->event_x, ev->event_y },
	.dblclick = 0
    };
    if (click.button == MB_LEFT)
    {
	if (ev->time - self->clicktime <= DBLCLICK_MS) click.dblclick = 1;
	self->clicktime = ev->time;
    }
    Widget_clicked(self, &click);
}

static void buttonrelease(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    if (!self->mainWidget) return;
    xcb_button_release_event_t *ev = args;
    MouseButton releasedButton = 1 << (ev->detail - 1);
    if ((int)self->anchorButton >= 0 && releasedButton & self->anchorButton)
    {
	self->anchorButton = (MouseButton)-1;
	self->anchorPos = (Pos){-1, -1};
    }
}

static void enter(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_enter_notify_event_t *ev = args;
    self->absMouse.x = ev->root_x;
    self->absMouse.y = ev->root_y;
    self->mouseUpdate.x = ev->event_x;
    self->mouseUpdate.y = ev->event_y;
    MouseButton heldButtons = ev->state >> 8;
    if ((int)self->anchorButton >= 0 && !(self->anchorButton & heldButtons))
    {
	self->anchorButton = (MouseButton)-1;
	self->anchorPos = (Pos){-1, -1};
    }
}

static void exposed(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_expose_event_t *ev = args;

    PSC_Log_fmt(PSC_L_DEBUG, "Expose 0x%x: { %u, %u, %u, %u }, count %u",
	    (unsigned)self->w, (unsigned)ev->x, (unsigned)ev->y,
	    (unsigned)ev->width, (unsigned)ev->height, (unsigned)ev->count);
    if (!self->mapped) return;
    if (self->mapped == 1)
    {
	Widget_showWindow(receiver);
	self->mapped = 2;
    }
    if (self->p)
    {
	if (self->ndamages < 0) return;
	if (self->ndamages == MAXDAMAGES) self->ndamages = -1;
	else self->damages[self->ndamages++] = (Rect){
	    .pos = { .x = ev->x, .y = ev->y },
	    .size = { .width = ev->width, .height = ev->height}};
    }
    else Widget_invalidateRegion(self,
	    (Rect){{ev->x, ev->y},{ev->width, ev->height}});
}

static void focusin(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    if (!self->focusWidget) return;
    Widget_activate(self->focusWidget);
}

static void focusout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    if (!self->focusWidget) return;
    Widget_deactivate(self->focusWidget);
}

static void keypress(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    if (!self->focusWidget) return;
    if (self->tooltipWindow && self->tooltipWindow->mapped)
    {
	Window_close(self->tooltipWindow);
    }

    XkbKeyEventArgs *ea = args;
    xkb_keysym_t key = ea->keysym;

    xkb_compose_state_feed(self->kbcompose, key);
    switch (xkb_compose_state_get_status(self->kbcompose))
    {
	case XKB_COMPOSE_COMPOSED:
	    key = xkb_compose_state_get_one_sym(self->kbcompose);
	    xkb_compose_state_reset(self->kbcompose);
	    break;

	case XKB_COMPOSE_CANCELLED:
	    xkb_compose_state_reset(self->kbcompose);
	    ATTR_FALLTHROUGH;

	case XKB_COMPOSE_COMPOSING:
	    return;

	default:
	    break;
    }

    KeyEvent event = {
	.codepoint = xkb_keysym_to_utf32(key),
	.keysym = key,
	.modifiers = ea->modifiers
    };
    Widget_keyPressed(self->focusWidget, &event);
}

static void leave(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_leave_notify_event_t *ev = args;
    if (ev->mode == XCB_NOTIFY_MODE_GRAB
	    || ev->mode == XCB_NOTIFY_MODE_UNGRAB) return;
    if ((int)self->anchorButton <= 0)
    {
	self->mouseUpdate.x = -1;
	self->mouseUpdate.y = -1;
    }
}

static void configureNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_configure_notify_event_t *ev = args;
    self->newSize = (Size){ ev->width, ev->height };
}

static void motionNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_motion_notify_event_t *ev = args;
    self->absMouse.x = ev->root_x;
    self->absMouse.y = ev->root_y;
    self->mouseUpdate.x = ev->event_x;
    self->mouseUpdate.y = ev->event_y;
    MouseButton heldButtons = ev->state >> 8;
    if (!self->anchorButton)
    {
	if (heldButtons)
	{
	    self->anchorButton = heldButtons;
	    self->anchorPos = self->mouse;
	}
    }
    else
    {
	if ((int)self->anchorButton >= 0
		&& !(heldButtons & self->anchorButton))
	{
	    self->anchorButton = (MouseButton)-1;
	    self->anchorPos = (Pos){-1, -1};
	}
    }
}

static void readWmState(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    Window *self = obj;
    if (error || !reply) goto error;

    xcb_get_property_reply_t *prop = reply;
    if (prop->type != A(WM_STATE)) goto error;

    uint32_t len = xcb_get_property_value_length(prop);
    if (len != 8) goto error;

    uint32_t *state = xcb_get_property_value(prop);
    switch (*state)
    {
	case WM_STATE_WITHDRAWN:
	    self->mapped = 0;
	    self->state = WS_NONE;
	    CHECK(xcb_delete_property(X11Adapter_connection(),
			self->w, A(WM_STATE)),
		    "Cannot delete WM_STATE on window 0x%x", self->w);
	    break;

	case WM_STATE_NORMAL:
	    self->state = WS_NORMAL;
	    break;

	case WM_STATE_ICONIC:
	    self->state = WS_MINIMIZED;
	    break;

	default:
	    goto error;
    }
    PSC_Log_fmt(PSC_L_DEBUG, "Window 0x%x new state: %u",
	    self->w, self->state);
    return;

error:
    CHECK(xcb_delete_property(X11Adapter_connection(),
		self->w, A(WM_STATE)),
	    "Cannot delete WM_STATE on window 0x%x", self->w);
    self->havewmstate = 0;
}

static void propertyNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_property_notify_event_t *ev = args;
    PSC_Event_raise(self->propertyChanged, ev->atom, ev);

    if (ev->atom == A(WM_STATE))
    {
	switch (ev->state)
	{
	    case XCB_PROPERTY_NEW_VALUE:
		self->havewmstate = 1;
		AWAIT(xcb_get_property(X11Adapter_connection(), 0, self->w,
			    ev->atom, ev->atom, 0, 2),
			self, readWmState);
		break;

	    case XCB_PROPERTY_DELETE:
		self->state = WS_NONE;
		self->havewmstate = 0;
		if (self->wantmap < 0)
		{
		    self->wantmap = 0;
		    if (self->closed) PSC_Event_raise(self->closed, 0, 0);
		}
		X11App_removeWindow(app(), self);
		break;

	    default:
		break;
	}
    }
}

static void clientmsg(void *receiver, void *sender, void *args)
{
    (void)sender;

    Window *self = receiver;
    xcb_client_message_event_t *ev = args;
    
    if (ev->data.data32[0] == A(WM_DELETE_WINDOW))
    {
	Window_close(self);
    }
    else if (ev->data.data32[0] == A(_NET_WM_PING))
    {
	xcb_connection_t *c = X11Adapter_connection();
	xcb_window_t root = X11Adapter_screen()->root;
	ev->window = root;
	CHECK(xcb_send_event(c, 0, root,
		    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *)ev),
		"Cannot answer _NET_WM_PING for 0x%x", (unsigned)self->w);
    }
}

static void requestError(void *receiver, void *sender, void *args)
{
    (void)sender;

    X11App *xapp = app();
    if (!xapp) PSC_Service_panic("BUG, received error without running app");
    Window *self = receiver;
    Widget *widget = Widget_cast(self);
    X11App_raiseError(xapp, self, widget, args);
}

static void doupdates(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    if (self->newSize.width && self->newSize.height)
    {
	Size oldsz = Widget_size(self);
	if (memcmp(&oldsz, &self->newSize, sizeof oldsz))
	{
	    Widget_setWindowSize(self, self->newSize);
	}
	self->newSize = (Size){0, 0};
    }
    if (!self->mainWidget || !self->mapped) return;
    if (memcmp(&self->mouse, &self->mouseUpdate, sizeof self->mouse))
    {
	self->mouse = self->mouseUpdate;
	if (self->mouse.x >= 0 && self->mouse.y >= 0
		&& self->tooltipWindow && self->tooltipWindow->mapped)
	{
	    Window_close(self->tooltipWindow);
	}
	Pos hoverPos;
	if (self->anchorPos.x >= 0 && self->anchorPos.y >= 0)
	{
	    hoverPos = self->anchorPos;
	}
	else
	{
	    hoverPos = self->mouse;
	}
	if (hoverPos.x >= 0 && hoverPos.y >= 0)
	{
	    Widget *hover = Widget_enterAt(self->mainWidget, hoverPos);
	    if (hover != self->hoverWidget)
	    {
		if (self->hoverWidget) Object_destroy(self->hoverWidget);
		if (!hover)
		{
		    self->hoverWidget = 0;
		    goto done;
		}
		else self->hoverWidget = Object_ref(hover);
	    }
	    if (self->anchorButton)
	    {
		if ((int)self->anchorButton == -1) self->anchorButton = 0;
		DragEvent event = {
		    .button = self->anchorButton,
		    .from = self->anchorPos,
		    .to = self->mouse
		};
		Widget_dragged(self->hoverWidget, &event);
	    }
	}
	else
	{
	    Object_destroy(self->hoverWidget);
	    self->hoverWidget = 0;
	    Widget_leave(self->mainWidget);
	}
	XCursor cursor = self->hoverWidget
	    ? Widget_cursor(self->hoverWidget)
	    : XC_LEFTPTR;
	if (cursor != self->cursor)
	{
	    CHECK(xcb_change_window_attributes(X11Adapter_connection(),
			self->w, XCB_CW_CURSOR, (uint32_t[]){
			X11Adapter_cursor(cursor) }),
		    "Cannot change cursor for 0x%x", (unsigned)self->w);
	    self->cursor = cursor;
	}
    }
done:
    Widget_draw(self);
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
    if (self->p && (ea->newSize.width > ea->oldSize.width
		|| ea->newSize.height > ea->oldSize.height))
    {
	xcb_connection_t *c = X11Adapter_connection();
	xcb_render_free_picture(c, self->src);
	xcb_free_pixmap(c, self->p);
	self->p = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 24, self->p, X11Adapter_screen()->root,
		    ea->newSize.width, ea->newSize.height),
		"Cannot create backing store pixmap for 0x%x",
		(unsigned)self->w);
	self->src = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, self->src, self->p,
		    X11Adapter_format(PICTFORMAT_RGB), 0, 0),
		"Cannot create XRender picture for backing store on 0x%x",
		(unsigned)self->w);
	Widget_setDrawable(self, self->p);
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
    else
    {
	self->haveMinSize = 0;
	if (!minSize.width) minSize.width = 1;
	if (!minSize.height) minSize.height = 1;
    }
    Size newSize = Widget_size(self);
    WindowFlags wtype = self->flags & WF_WINDOW_TYPE;
    int exactsize = wtype == WF_WINDOW_TOOLTIP || wtype == WF_WINDOW_MENU;
    if (exactsize || minSize.width > newSize.width)
    {
	newSize.width = minSize.width;
    }
    if (exactsize || minSize.height > newSize.height)
    {
	newSize.height = minSize.height;
    }
    Widget_setSize(self, newSize);
    WMSizeHints hints = {
	.flags = WM_SIZE_HINT_P_MIN_SIZE,
	.min_width = minSize.width,
	.min_height = minSize.height
    };
    if (self->flags & WF_FIXED_SIZE)
    {
	hints.flags |= WM_SIZE_HINT_P_MAX_SIZE;
	hints.max_width = minSize.width;
	hints.max_height = minSize.height;
    }
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
    X11App_addWindow(app(), self);
    PSC_Log_fmt(PSC_L_DEBUG, "Window 0x%x mapped", (unsigned)self->w);
}

static void unmapped(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Window *self = receiver;
    self->mapped = 0;
    Widget_hideWindow(self);
    PSC_Log_fmt(PSC_L_DEBUG, "Window 0x%x unmapped", (unsigned)self->w);
    if (!self->havewmstate && self->wantmap < 0)
    {
	self->wantmap = 0;
	if (self->closed) PSC_Event_raise(self->closed, 0, 0);
	X11App_removeWindow(app(), self);
    }
}

static void destroy(void *window)
{
    Window *self = window;
    Object_destroy(self->tooltipWindow);
    PSC_Event_unregister(Widget_sizeChanged(self), self, sizeChanged, 0);
    PSC_Event_unregister(X11Adapter_eventsDone(), self, doupdates, 0);
    PSC_Event_unregister(X11Adapter_unmapNotify(), self,
	    unmapped, self->w);
    PSC_Event_unregister(X11Adapter_motionNotify(), self,
	    motionNotify, self->w);
    PSC_Event_unregister(X11Adapter_mapNotify(), self,
	    mapped, self->w);
    PSC_Event_unregister(X11Adapter_leave(), self,
	    leave, self->w);
    PSC_Event_unregister(X11Adapter_focusout(), self,
	    focusout, self->w);
    PSC_Event_unregister(X11Adapter_focusin(), self,
	    focusin, self->w);
    PSC_Event_unregister(X11Adapter_expose(), self,
	    exposed, self->w);
    PSC_Event_unregister(X11Adapter_enter(), self,
	    enter, self->w);
    PSC_Event_unregister(X11Adapter_configureNotify(), self,
	    configureNotify, self->w);
    PSC_Event_unregister(X11Adapter_clientmsg(), self,
	    clientmsg, self->w);
    PSC_Event_unregister(X11Adapter_buttonrelease(), self,
	    buttonrelease, self->w);
    PSC_Event_unregister(X11Adapter_buttonpress(), self,
	    buttonpress, self->w);
    PSC_Event_unregister(X11Adapter_requestError(), self,
	    requestError, self->w);
    xkb_compose_state_unref(self->kbcompose);
    XSelection_destroy(self->clipboard);
    XSelection_destroy(self->primary);
    PSC_Event_destroy(self->propertyChanged);
    PSC_Event_destroy(self->closed);
    Object_destroy(self->hoverWidget);
    Object_destroy(self->focusWidget);
    if ((self->flags & WF_WINDOW_TYPE) != WF_WINDOW_MENU)
    {
	Object_destroy(self->mainWidget);
    }
    xcb_connection_t *c = X11Adapter_connection();
    if (self->p)
    {
	xcb_render_free_picture(c, self->dst);
	xcb_render_free_picture(c, self->src);
	xcb_free_pixmap(c, self->p);
    }
    xcb_destroy_window(c, self->w);
    free(self->iconName);
    free(self->title);
    free(self);
}


Window *Window_createBase(void *derived, const char *name,
	WindowFlags flags, void *parent)
{
    WindowFlags wtype = flags & WF_WINDOW_TYPE;
    Window *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    void *owner = parent;
    if (wtype == WF_WINDOW_TOOLTIP) owner = 0;
    CREATEBASE(Widget, name, owner);
    self->closed = PSC_Event_create(self);
    self->propertyChanged = PSC_Event_create(self);
    self->mouseUpdate = (Pos){-1, -1};
    self->anchorPos = (Pos){-1, -1};
    self->hideState = WS_MINIMIZED;
    Widget *parentWidget = parent ? Widget_tryCast(parent) : 0;
    Window *parentWin = 0;
    if (parentWidget)
    {
	Widget_setContainer(self, parentWidget);
	parentWin = Window_fromWidget(parentWidget);
    }

    xcb_connection_t *c = X11Adapter_connection();
    self->w = xcb_generate_id(c);
    PSC_Event_register(X11Adapter_requestError(), self, requestError, self->w);

    xcb_screen_t *s = X11Adapter_screen();
    uint32_t mask;
    uint32_t values[2] = { 1,
	XCB_EVENT_MASK_BUTTON_MOTION
	    | XCB_EVENT_MASK_BUTTON_PRESS
	    | XCB_EVENT_MASK_BUTTON_RELEASE
	    | XCB_EVENT_MASK_ENTER_WINDOW
	    | XCB_EVENT_MASK_EXPOSURE
	    | XCB_EVENT_MASK_FOCUS_CHANGE
	    | XCB_EVENT_MASK_KEY_PRESS
	    | XCB_EVENT_MASK_LEAVE_WINDOW
	    | XCB_EVENT_MASK_POINTER_MOTION
	    | XCB_EVENT_MASK_PROPERTY_CHANGE
	    | XCB_EVENT_MASK_STRUCTURE_NOTIFY };
    if (wtype == WF_WINDOW_TOOLTIP || wtype == WF_WINDOW_MENU)
    {
	mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    }
    else
    {
	mask = XCB_CW_EVENT_MASK;
	values[0] = values[1];
    }
    CHECK(xcb_create_window(c, XCB_COPY_FROM_PARENT, self->w, s->root,
		0, 0, 1, 1, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		s->root_visual, mask, values),
	    "Cannot create window 0x%x", (unsigned)self->w);
    if (parentWin && wtype != WF_WINDOW_MENU)
    {
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW,
		    32, 1, &parentWin->w),
		"Cannot set transient state for 0x%x", (unsigned)self->w);
    }
    if (wtype == WF_WINDOW_NORMAL || wtype == WF_WINDOW_DIALOG
	    || (flags & WF_ALWAYS_CLASS))
    {
	size_t sz;
	const char *wmclass = X11Adapter_wmClass(&sz);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, sz, wmclass),
		"Cannot set window class for 0x%x", (unsigned)self->w);
    }
    if (wtype == WF_WINDOW_NORMAL || wtype == WF_WINDOW_DIALOG)
    {
	self->kbcompose = xkb_compose_state_new(
		X11Adapter_kbdcompose(), XKB_COMPOSE_STATE_NO_FLAGS);
	WMHints hints = {
	    .flags = WM_HINT_INPUT | WM_HINT_STATE,
	    .input = !(flags & WF_REJECT_FOCUS),
	    .state = WM_STATE_NORMAL
	};
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32,
		    sizeof hints >> 2, &hints),
		"Cannot set window manager hints on 0x%x", (unsigned)self->w);
	X11App_setWmProperties(self);
	xcb_atom_t protocols[3] = {
	    A(_NET_WM_PING), A(WM_DELETE_WINDOW), A(WM_TAKE_FOCUS) };
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    A(WM_PROTOCOLS), XCB_ATOM_ATOM, 32,
		    (flags & WF_REJECT_FOCUS) ? 3 : 2, protocols),
		"Cannot set supported protocols on 0x%x", (unsigned)self->w);
	xcb_atom_t wmtype;
	switch (wtype)
	{
	    case WF_WINDOW_DIALOG:
		wmtype = A(_NET_WM_WINDOW_TYPE_DIALOG);
		break;

	    default:
		wmtype = A(_NET_WM_WINDOW_TYPE_NORMAL);
		break;
	}
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    A(_NET_WM_WINDOW_TYPE), XCB_ATOM_ATOM, 32, 1, &wmtype),
		"Cannot set window type for 0x%x", (unsigned)self->w);
	MwmHints mwmhints = {
	    .flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS,
	    .functions = MWM_FUNC_ANY,
	    .decorations = MWM_DECOR_ANY,
	    .input_mode = 0,
	    .status = 0
	};
	if (wtype == WF_WINDOW_DIALOG)
	{
	    mwmhints.functions &= ~(MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE);
	    mwmhints.decorations &= ~(MWM_DECOR_MINIMIZE);
	}
	if (flags & WF_FIXED_SIZE)
	{
	    mwmhints.functions &= ~(MWM_FUNC_RESIZE | MWM_FUNC_MAXIMIZE);
	    mwmhints.decorations &= ~(MWM_DECOR_RESIZEH | MWM_DECOR_MAXIMIZE);
	}
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->w,
		    A(_MOTIF_WM_HINTS), A(_MOTIF_WM_HINTS), 32,
		    sizeof mwmhints >> 2, &mwmhints),
		"Cannot set MWM hints on 0x%x", (unsigned)self->w);
	Widget_setBackground(self, 1, COLOR_BG_NORMAL);
    }
    if (wtype != WF_WINDOW_TOOLTIP)
    {
	int backingstore = XRdb_bool(X11Adapter_resources(),
		XRdbKey(Widget_resname(self), "backingStore"),
		XRQF_OVERRIDES, 1);
	if (backingstore)
	{
	    self->p = xcb_generate_id(c);
	    CHECK(xcb_create_pixmap(c, 24, self->p, s->root, 1, 1),
		    "Cannot create backing store pixmap for 0x%x",
		    (unsigned)self->w);
	    self->src = xcb_generate_id(c);
	    CHECK(xcb_render_create_picture(c, self->src, self->p,
			X11Adapter_format(PICTFORMAT_RGB), 0, 0),
		    "Cannot create XRender picture for backing store on 0x%x",
		    (unsigned)self->w);
	    self->dst = xcb_generate_id(c);
	    CHECK(xcb_render_create_picture(c, self->dst, self->w,
			X11Adapter_rootformat(), 0, 0),
		    "Cannot create XRender picture for window surface on 0x%x",
		    (unsigned)self->w);
	}
	else
	{
	    PSC_Log_fmt(PSC_L_INFO, "Disabled backing store for window 0x%x",
		    (unsigned)self->w);
	}
    }

    self->flags = flags;
    self->borderpixel = (uint32_t)-1;

    Widget_setSize(self, (Size){1, 1});
    Widget_setDrawable(self, self->p ? self->p : self->w);

    PSC_Event_register(X11Adapter_buttonpress(), self,
	    buttonpress, self->w);
    PSC_Event_register(X11Adapter_buttonrelease(), self,
	    buttonrelease, self->w);
    PSC_Event_register(X11Adapter_clientmsg(), self,
	    clientmsg, self->w);
    PSC_Event_register(X11Adapter_configureNotify(), self,
	    configureNotify, self->w);
    PSC_Event_register(X11Adapter_enter(), self,
	    enter, self->w);
    PSC_Event_register(X11Adapter_expose(), self,
	    exposed, self->w);
    PSC_Event_register(X11Adapter_focusin(), self,
	    focusin, self->w);
    PSC_Event_register(X11Adapter_focusout(), self,
	    focusout, self->w);
    PSC_Event_register(X11Adapter_keypress(), self,
	    keypress, self->w);
    PSC_Event_register(X11Adapter_leave(), self,
	    leave, self->w);
    PSC_Event_register(X11Adapter_mapNotify(), self,
	    mapped, self->w);
    PSC_Event_register(X11Adapter_motionNotify(), self,
	    motionNotify, self->w);
    PSC_Event_register(X11Adapter_propertyNotify(), self,
	    propertyNotify, self->w);
    PSC_Event_register(X11Adapter_unmapNotify(), self,
	    unmapped, self->w);
    PSC_Event_register(X11Adapter_eventsDone(), self,
	    doupdates, 0);
    PSC_Event_register(Widget_sizeChanged(self), self,
	    sizeChanged, 0);

    return self;
}

Window *Window_fromWidget(void *widget)
{
    Window *w = Object_cast(widget);
    while (!w)
    {
	widget = Widget_container(widget);
	if (!widget) break;
	w = Object_cast(widget);
    }
    return w;
}

xcb_window_t Window_id(const void *self)
{
    const Window *w = Object_instance(self);
    return w->w;
}

PSC_Event *Window_closed(void *self)
{
    Window *w = Object_instance(self);
    return w->closed;
}

PSC_Event *Window_propertyChanged(void *self)
{
    Window *w = Object_instance(self);
    return w->propertyChanged;
}

void Window_addFlags(void *self, WindowFlags flags)
{
    Window *w = Object_instance(self);
    flags &= ~WF_WINDOW_TYPE;
    w->flags |= flags;
}

void Window_removeFlags(void *self, WindowFlags flags)
{
    Window *w = Object_instance(self);
    flags = ~flags | WF_WINDOW_TYPE;
    w->flags &= flags;
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
	char *latintitle = LATIN1(w->title);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		    XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
		    8, strlen(latintitle), latintitle),
		"Cannot set latin1 title for 0x%x", (unsigned)w->w);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		    A(_NET_WM_NAME), A(UTF8_STRING),
		    8, strlen(w->title), w->title),
		"Cannot set utf8 title for 0x%x", (unsigned)w->w);
	free(latintitle);
    }
    else
    {
	w->title = 0;
	CHECK(xcb_delete_property(c, w->w, XCB_ATOM_WM_NAME),
		"Cannot delete latin1 title for 0x%x", (unsigned)w->w);
	CHECK(xcb_delete_property(c, w->w, A(_NET_WM_NAME)),
		"Cannot delete utf8 title for 0x%x", (unsigned)w->w);
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
	char *latinIconName = LATIN1(newname);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		    XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8,
		    strlen(latinIconName), latinIconName),
		"Cannot set latin1 icon name for 0x%x", (unsigned)w->w);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w->w,
		    A(_NET_WM_ICON_NAME), A(UTF8_STRING), 8,
		    strlen(newname), newname),
		"Cannot set utf8 icon name for 0x%x", (unsigned)w->w);
	free(latinIconName);
    }
    else
    {
	CHECK(xcb_delete_property(c, w->w, XCB_ATOM_WM_ICON_NAME),
		"Cannot delete latin1 icon name for 0x%x", (unsigned)w->w);
	CHECK(xcb_delete_property(c, w->w, A(_NET_WM_ICON_NAME)),
		"Cannot delete utf8 icon name for 0x%x", (unsigned)w->w);
    }
}

void *Window_mainWidget(const void *self)
{
    Window *w = Object_instance(self);
    return w->mainWidget;
}

static void setBorderColor(void *obj, Color color, uint32_t pixel)
{
    (void)color;

    Window *self = obj;
    self->borderpixel = pixel;
    CHECK(xcb_change_window_attributes(X11Adapter_connection(), self->w,
		XCB_CW_BORDER_PIXEL, (const void *)&self->borderpixel),
	    "Cannot set border color for 0x%x", (unsigned)self->w);
}

void Window_setMainWidget(void *self, void *widget)
{
    Window *w = Object_instance(self);
    WindowFlags wtype = w->flags & WF_WINDOW_TYPE;
    if (w->mainWidget)
    {
	PSC_Event_unregister(Widget_sizeRequested(w->mainWidget), w,
		sizeRequested, 0);
	Widget_setContainer(w->mainWidget, 0);
	if (wtype != WF_WINDOW_MENU) Object_destroy(w->mainWidget);
    }
    w->mainWidget = wtype == WF_WINDOW_MENU ? widget : Object_ref(widget);
    if (widget)
    {
	Widget_setContainer(widget, w);
	if (wtype == WF_WINDOW_TOOLTIP || wtype == WF_WINDOW_MENU)
	{
	    if (w->borderpixel != (uint32_t)-1)
	    {
		X11Adapter_unmapColor(w->borderpixel);
		w->borderpixel = (uint32_t)-1;
	    }
	    Color bc = Widget_color(widget, w->flags & WF_POS_PARENTWIDGET ?
		    COLOR_ACTIVE : wtype == WF_WINDOW_TOOLTIP ?
		    COLOR_BORDER_TOOLTIP : COLOR_BORDER);
	    X11Adapter_mapColor(self, setBorderColor, bc);
	}
	Font *font = Widget_font(w);
	if (!font) Widget_setFontResName(w, 0, 0, 0);
	else Widget_offerFont(widget, font);
	PSC_Event_register(Widget_sizeRequested(widget), w, sizeRequested, 0);
	w->haveMinSize = 0;
	Widget_setSize(w, (Size){1, 1});
	sizeRequested(w, 0, 0);
    }
}

void Window_setFocusWidget(void *self, void *widget)
{
    Window *w = Object_instance(self);
    if (w->focusWidget)
    {
	if (w->focusWidget == widget) return;
	Widget *prev = w->focusWidget;
	w->focusWidget = 0;
	Widget_unfocus(prev);
	Object_destroy(prev);
    }
    if (widget) w->focusWidget = Object_ref(widget);
}

xcb_atom_t Window_takeProperty(void *self)
{
    Window *w = Object_instance(self);
    for (int i = 0; i < 16; ++i)
    {
	if (!(w->tmpProperties & (1 << i)))
	{
	    w->tmpProperties |= (1 << i);
	    return i+1;
	}
    }
    return 0;
}

void Window_returnProperty(void *self, xcb_atom_t property)
{
    if (property < 1 || property > 16) return;
    Window *w = Object_instance(self);
    w->tmpProperties &= ~(1 << (property-1));
}

XSelection *Window_primary(void *self)
{
    Window *w = Object_instance(self);
    if (!w->primary) w->primary = XSelection_create(w, XSN_PRIMARY);
    return w->primary;
}

XSelection *Window_clipboard(void *self)
{
    Window *w = Object_instance(self);
    if (!w->clipboard) w->clipboard = XSelection_create(w, XSN_CLIPBOARD);
    return w->clipboard;
}

void Window_close(void *self)
{
    Window *w = Object_instance(self);
    w->wantmap = -1;
    xcb_connection_t *c = X11Adapter_connection();
    xcb_window_t root = X11Adapter_screen()->root;
    CHECK(xcb_unmap_window(c, w->w),
	    "Cannot unmap window 0x%x", (unsigned)w->w);
    xcb_unmap_notify_event_t ev = {
	.response_type = XCB_UNMAP_NOTIFY,
	.pad0 = 0,
	.sequence = 0,
	.event = root,
	.window = w->w,
	.from_configure = 0,
	.pad1 = {0, 0, 0}
    };
    CHECK(xcb_send_event(c, 0, root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *)&ev),
	    "Cannot send unmap notify for window 0x%x", (unsigned)w->w);
    PSC_Log_fmt(PSC_L_DEBUG, "Unmapping window 0x%x", (unsigned)w->w);
}

void Window_showTooltip(void *self, void *widget, void *parentWidget)
{
    Window *w = Object_instance(self);
    if (!w->tooltipWindow)
    {
	w->tooltipWindow = Window_create(0, WF_WINDOW_TOOLTIP, w);
	Widget_setFontResName(w->tooltipWindow, "tooltipFont", 0, 0);
    }
    Widget_setContainer(w->tooltipWindow, parentWidget);
    Window_setMainWidget(w->tooltipWindow, widget);
    Widget_show(w->tooltipWindow);
}

void Window_invalidateHover(void *self)
{
    Window *w = Object_instance(self);
    if (w->anchorPos.x < 0) w->mouse = (Pos){-1, -1};
}

void Window_showWaitCursor(void *self)
{
    Window *w = Object_instance(self);
    xcb_connection_t *c = X11Adapter_connection();
    w->cursor = XC_WATCH;
    CHECK(xcb_change_window_attributes(c, w->w, XCB_CW_CURSOR,
		(uint32_t[]){ X11Adapter_cursor(w->cursor) }),
	    "Cannot change cursor for 0x%x", (unsigned)w->w);
    xcb_flush(c);
}

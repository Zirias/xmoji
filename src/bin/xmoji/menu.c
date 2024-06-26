#include "menu.h"

#include "button.h"
#include "command.h"
#include "window.h"

#include <poser/core.h>
#include <stdlib.h>

C_CLASS_DECL(Font);

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int show(void *obj);
static int hide(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaMenu mo = MetaMenu_init(
	expose, draw, show, hide,
	0, 0, 0, leave, 0, 0, 0, 0, setFont, childAt,
	minSize, 0, clicked, 0,
	"Menu", destroy);

typedef enum MenuItemType
{
    MIT_ITEM,
    MIT_MENU,
    MIT_SEPARATOR
} MenuItemType;

typedef struct MenuItem
{
    void *content;
    void *command;
    MenuItemType type;
} MenuItem;

struct Menu
{
    Object base;
    Window *window;
    PSC_List *items;
    MenuItem *hoverItem;
    Size minSize;
};

static void destroy(void *obj)
{
    Menu *self = obj;
    PSC_List_destroy(self->items);
    free(self);
}

static void expose(void *obj, Rect region)
{
    Menu *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *item = PSC_ListIterator_current(i);
	Widget_invalidateRegion(item->content, region);
    }
    PSC_ListIterator_destroy(i);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    Menu *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    int rc = 0;
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *item = PSC_ListIterator_current(i);
	if (Widget_draw(item->content) < 0) rc = -1;
    }
    PSC_ListIterator_destroy(i);
    return rc;
}

static int show(void *obj)
{
    Menu *self = obj;
    int rc = 0;
    Object_bcall(rc, Widget, show, self);
    return rc < 0 ? rc : Widget_show(self->window);
}

static int hide(void *obj)
{
    leave(obj);
    Menu *self = obj;
    CHECK(xcb_ungrab_pointer(X11Adapter_connection(), XCB_CURRENT_TIME),
	    "Cannot ungrab pointer for 0x%x", Window_id(self->window));
    Window_close(self->window);
    int rc = 0;
    Object_bcall(rc, Widget, hide, self);
    return rc;
}

static Size minSize(const void *obj)
{
    const Menu *self = Object_instance(obj);
    return self->minSize;
}

static void leave(void *obj)
{
    Menu *self = Object_instance(obj);
    if (self->hoverItem)
    {
	Widget_leave(self->hoverItem->content);
	self->hoverItem = 0;
    }
}

static void setFont(void *obj, Font *font)
{
    Menu *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *item = PSC_ListIterator_current(i);
	Widget_offerFont(item->content, font);
    }
    PSC_ListIterator_destroy(i);
}

static MenuItem *itemAt(Menu *self, Pos pos)
{
    MenuItem *item = 0;
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *current = PSC_ListIterator_current(i);
	Rect childGeom = Widget_geometry(current->content);
	if (pos.x >= childGeom.pos.x
		&& pos.x < childGeom.pos.x + childGeom.size.width
		&& pos.y >= childGeom.pos.y
		&& pos.y < childGeom.pos.y + childGeom.size.height)
	{
	    item = current;
	    break;
	}
    }
    PSC_ListIterator_destroy(i);
    return item;
}

static Widget *childAt(void *obj, Pos pos)
{
    Menu *self = Object_instance(obj);
    MenuItem *item = itemAt(self, pos);
    if (item != self->hoverItem)
    {
	if (self->hoverItem) Widget_leave(self->hoverItem->content);
	self->hoverItem = item;
    }
    if (item) return Widget_enterAt(item->content, pos);
    return Widget_cast(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    Menu *self = Object_instance(obj);
    Widget_hide(self);
    MenuItem *item = itemAt(self, event->pos);
    if (item) return Widget_clicked(item->content, event);
    return 1;
}

Menu *Menu_createBase(void *derived, const char *name, void *parent)
{
    Menu *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->window = 0;
    self->items = PSC_List_create();
    self->hoverItem = 0;
    self->minSize = (Size){0, 0};

    Widget_setPadding(self, (Box){0, 0, 0, 0});

    return self;
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Menu *self = receiver;
    Pos origin = Widget_origin(self);
    self->minSize = (Size){0, 0};
    PSC_ListIterator *i = PSC_List_iterator(self->items);
    Pos itemOrigin = origin;
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *item = PSC_ListIterator_current(i);
	Widget_setOrigin(item->content, itemOrigin);
	Size itemSize = Widget_minSize(item->content);
	itemOrigin.y += itemSize.height;
	self->minSize.height += itemSize.height;
	if (itemSize.width > self->minSize.width)
	{
	    self->minSize.width = itemSize.width;
	}
    }
    if (!self->minSize.width || !self->minSize.height) goto done;
    Widget_requestSize(self);
    while (PSC_ListIterator_moveNext(i))
    {
	MenuItem *item = PSC_ListIterator_current(i);
	Size itemSize = Widget_minSize(item->content);
	itemSize.width = self->minSize.width;
	Widget_setSize(item->content, itemSize);
    }
done:
    PSC_ListIterator_destroy(i);
}

void Menu_addItem(void *self, void *command)
{
    Menu *m = Object_instance(self);
    MenuItem *item = PSC_malloc(sizeof *item);
    item->content = Button_create(Widget_name(m), m);
    item->command = command;
    item->type = MIT_ITEM;
    PSC_Event_register(Widget_sizeRequested(item->content), self,
	    sizeRequested, 0);
    PSC_List_append(m->items, item, free);
    Button_attachCommand(item->content, command);
    Button_setBorderWidth(item->content, 0);
    Button_setColors(item->content, COLOR_BG_NORMAL, COLOR_BG_ACTIVE);
    Button_setLabelPadding(item->content, (Box){20, 1, 16, 1});
    Button_setLabelAlign(item->content, AV_MIDDLE);
    Button_setMinWidth(item->content, 0);
    Widget_setPadding(item->content, (Box){0, 0, 0, 0});
    Widget_setExpand(item->content, EXPAND_X);
    Widget_setContainer(item->content, self);
    Widget_show(item->content);
    sizeRequested(m, 0, 0);
}

static void pointerGrabbed(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    if (error || !reply) goto error;
    xcb_grab_pointer_reply_t *grab = reply;
    if (grab->status == XCB_GRAB_STATUS_SUCCESS) return;
error:
    Widget_hide(obj);
}

void Menu_popup(void *self, void *widget)
{
    Menu *m = Object_instance(self);
    if (!m->window)
    {
	m->window = Window_create(Widget_name(m), WF_WINDOW_MENU, widget);
	Window_setMainWidget(m->window, self);
    }
    else Widget_setContainer(m->window, widget);
    Widget_show(m);
    AWAIT(xcb_grab_pointer(X11Adapter_connection(), 0, Window_id(m->window),
		XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_BUTTON_PRESS
		| XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_ENTER_WINDOW
		| XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE,
		XCB_CURRENT_TIME),
	    m, pointerGrabbed);
}

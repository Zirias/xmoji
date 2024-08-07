#include "tabbox.h"

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

static MetaTabBox mo = MetaTabBox_init(
	expose, draw, show, hide,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, 0,
	"TabBox", destroy);

typedef struct Tab
{
    TabBox *box;
    Widget *buttonWidget;
    Widget *contentWidget;
    Rect tabGeom;
    Size buttonMinSize;
    Size contentMinSize;
    int index;
} Tab;

struct TabBox
{
    Object base;
    PSC_List *tabs;
    Widget *hovered;
    Size minSize;
    int currentIndex;
    int hoverIndex;
    int shown;
};

static void destroy(void *obj)
{
    TabBox *self = obj;
    PSC_List_destroy(self->tabs);
    free(self);
}

static void layout(TabBox *self)
{
    if (!self->shown) return;
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    Size barSize = (Size){0, 0};
    Size contentSize = (Size){0, 0};
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	barSize.width += tab->buttonMinSize.width + 1;
	if (tab->buttonMinSize.height > barSize.height)
	{
	    barSize.height = tab->buttonMinSize.height;
	}
	if (tab->contentMinSize.width > contentSize.width)
	{
	    contentSize.width = tab->contentMinSize.width;
	}
	if (tab->contentMinSize.height > contentSize.height)
	{
	    contentSize.height = tab->contentMinSize.height;
	}
    }
    barSize.width -= 1;
    barSize.height += 2;
    Size minSize = barSize;
    minSize.height += contentSize.height + 1;
    if (contentSize.width > minSize.width) minSize.width = contentSize.width;
    if (memcmp(&minSize, &self->minSize, sizeof minSize))
    {
	self->minSize = minSize;
	Widget_requestSize(self);
    }
    Size realSize = Widget_size(self);
    Box padding = Widget_padding(self);
    realSize.width -= padding.left + padding.right;
    realSize.height -= padding.top + padding.bottom;
    Pos origin = Widget_contentOrigin(self, realSize);
    realSize.height -= barSize.height + 1;
    Pos contentOrigin = origin;
    contentOrigin.y += barSize.height + 1;
    Pos buttonOrigin = origin;
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	Size buttonSize = tab->buttonMinSize;
	Pos buttonPos = buttonOrigin;
	buttonPos.y += 2;
	Widget_setSize(tab->buttonWidget, buttonSize);
	Widget_setOrigin(tab->buttonWidget, buttonPos);
	buttonSize.height += 2;
	tab->tabGeom = (Rect){buttonOrigin, buttonSize};
	++tab->tabGeom.size.height;
	buttonOrigin.x += buttonSize.width + 1;
	Widget_setOrigin(tab->contentWidget, contentOrigin);
    }
    PSC_ListIterator_destroy(i);
    Tab *tab = PSC_List_at(self->tabs, self->currentIndex);
    Widget_setSize(tab->contentWidget, realSize);
    Widget_invalidate(self);
}

static void updateHover(TabBox *self, int index)
{
    if (index == self->hoverIndex) return;
    if (self->hoverIndex >= 0)
    {
	Tab *tab = PSC_List_at(self->tabs, self->hoverIndex);
	Widget_invalidateRegion(self, tab->tabGeom);
    }
    self->hoverIndex = index;
    if (self->hoverIndex >= 0)
    {
	Tab *tab = PSC_List_at(self->tabs, self->hoverIndex);
	Widget_invalidateRegion(self, tab->tabGeom);
    }
}

static void expose(void *obj, Rect region)
{
    TabBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	Widget_invalidateRegion(tab->buttonWidget, region);
	if (tab->index == self->currentIndex)
	{
	    Widget_invalidateRegion(tab->contentWidget, region);
	}
    }
    PSC_ListIterator_destroy(i);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    TabBox *self = Object_instance(obj);
    int rc = 0;
    xcb_connection_t *c = 0;
    if (picture)
    {
	c = X11Adapter_connection();
	Tab *tab = PSC_List_at(self->tabs, self->currentIndex);
	Rect tabGeom = Widget_geometry(tab->contentWidget);
	Pos barPos = Widget_origin(self);
	Box padding = Widget_padding(self);
	barPos.x += padding.left;
	barPos.y += padding.top;
	Color color = Widget_color(self, COLOR_BG_BELOW);
	xcb_rectangle_t rect = { barPos.x, barPos.y, tabGeom.size.width,
	    tabGeom.pos.y - barPos.y };
	if (Widget_isDamaged(self, (Rect){{rect.x, rect.y},
		    {rect.width, rect.height}}))
	{
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			picture, Color_xcb(color), 1, &rect),
		    "Cannot draw tab bar background on 0x%x",
		    (unsigned)picture);
	}
    }
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	if (picture)
	{
	    Color color = Widget_color(self, COLOR_BG_ABOVE);
	    xcb_rectangle_t rect = { tab->tabGeom.pos.x, tab->tabGeom.pos.y,
		tab->tabGeom.size.width, tab->tabGeom.size.height };
	    if (tab->index == self->hoverIndex)
	    {
		color = Widget_color(self, COLOR_BG_ACTIVE);
	    }
	    else if (tab->index == self->currentIndex)
	    {
		color = Widget_color(self, COLOR_BG_NORMAL);
	    }
	    else
	    {
		rect.y += 2;
		rect.height -= 3;
	    }
	    if (Widget_isDamaged(self, (Rect){{rect.x, rect.y},
			{rect.width, rect.height}}))
	    {
		CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			    picture, Color_xcb(color), 1, &rect),
			"Cannot draw tab background on 0x%x",
			(unsigned)picture);
	    }
	}
	if (Widget_draw(tab->buttonWidget) < 0) rc = -1;
	if (tab->index == self->currentIndex)
	{
	    if (Widget_draw(tab->contentWidget) < 0) rc = -1;
	}
    }
    PSC_ListIterator_destroy(i);
    return rc;
}

static int show(void *obj)
{
    TabBox *self = Object_instance(obj);
    self->shown = 1;
    layout(self);
    int rc = 1;
    Object_bcall(rc, Widget, show, self);
    return rc;
}

static int hide(void *obj)
{
    TabBox *self = Object_instance(obj);
    self->shown = 0;
    int rc = 1;
    Object_bcall(rc, Widget, hide, self);
    return rc;
}

static Size minSize(const void *obj)
{
    const TabBox *self = Object_instance(obj);
    return self->minSize;
}

static void leave(void *obj)
{
    TabBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	Widget_leave(tab->buttonWidget);
	Widget_leave(tab->contentWidget);
    }
    PSC_ListIterator_destroy(i);
    self->hovered = 0;
    updateHover(self, -1);
}

static void unselect(void *obj)
{
    TabBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	Widget_unselect(tab->buttonWidget);
	Widget_unselect(tab->contentWidget);
    }
    PSC_ListIterator_destroy(i);
}

static void setFont(void *obj, Font *font)
{
    TabBox *self = Object_instance(obj);
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	Tab *tab = PSC_ListIterator_current(i);
	Widget_offerFont(tab->buttonWidget, font);
	Widget_offerFont(tab->contentWidget, font);
    }
    PSC_ListIterator_destroy(i);
}

static Widget *childAt(void *obj, Pos pos)
{
    TabBox *self = Object_instance(obj);
    Widget *child = 0;
    if (self->currentIndex < 0) return child;
    Tab *tab = PSC_List_at(self->tabs, self->currentIndex);
    Rect contentGeom = Widget_geometry(tab->contentWidget);
    if (Rect_containsPos(contentGeom, pos))
    {
	child = Widget_enterAt(tab->contentWidget, pos);
	updateHover(self, -1);
    }
    else
    {
	int tabHovered = 0;
	PSC_ListIterator *i = PSC_List_iterator(self->tabs);
	while (PSC_ListIterator_moveNext(i))
	{
	    tab = PSC_ListIterator_current(i);
	    if (Rect_containsPos(tab->tabGeom, pos))
	    {
		updateHover(self, tab->index);
		tabHovered = 1;
		Rect buttonGeom = Widget_geometry(tab->buttonWidget);
		if (Rect_containsPos(buttonGeom, pos))
		{
		    child = Widget_enterAt(tab->buttonWidget, pos);
		}
		break;
	    }
	}
	PSC_ListIterator_destroy(i);
	if (tabHovered) Widget_setCursor(self, XC_HAND);
	else
	{
	    Widget_setCursor(self, XC_LEFTPTR);
	    updateHover(self, -1);
	}
    }
    if (child != self->hovered)
    {
	if (self->hovered) Widget_leave(self->hovered);
	self->hovered = child;
    }
    return child ? child : Widget_cast(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    TabBox *self = Object_instance(obj);
    if (self->currentIndex < 0) return 0;
    Tab *tab = PSC_List_at(self->tabs, self->currentIndex);
    Rect contentGeom = Widget_geometry(tab->contentWidget);
    if (Rect_containsPos(contentGeom, event->pos))
    {
	return Widget_clicked(tab->contentWidget, event);
    }
    int handled = 0;
    PSC_ListIterator *i = PSC_List_iterator(self->tabs);
    while (PSC_ListIterator_moveNext(i))
    {
	tab = PSC_ListIterator_current(i);
	if (Rect_containsPos(tab->tabGeom, event->pos))
	{
	    Rect buttonGeom = Widget_geometry(tab->buttonWidget);
	    if (Rect_containsPos(buttonGeom, event->pos))
	    {
		handled = Widget_clicked(tab->buttonWidget, event);
	    }
	    if (event->button == MB_LEFT)
	    {
		self->currentIndex = tab->index;
		layout(self);
		handled = 1;
	    }
	    break;
	}
    }
    PSC_ListIterator_destroy(i);
    return handled;
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    layout(receiver);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)args;

    Tab *tab = receiver;
    Widget *widget = sender;
    Size *oldMinSize;
    if (widget == tab->buttonWidget) oldMinSize = &tab->buttonMinSize;
    else if (widget == tab->contentWidget) oldMinSize = &tab->contentMinSize;
    else return;
    Size minSize = Widget_minSize(widget);
    if (memcmp(oldMinSize, &minSize, sizeof *oldMinSize))
    {
	*oldMinSize = minSize;
	layout(tab->box);
    }
}

TabBox *TabBox_createBase(void *derived, const char *name, void *parent)
{
    TabBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->tabs = PSC_List_create();
    self->hovered = 0;
    self->minSize = (Size){0, 0};
    self->currentIndex = -1;
    self->hoverIndex = -1;
    self->shown = 0;

    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);
    return self;
}

static void destroyTab(void *obj)
{
    if (!obj) return;
    Tab *self = obj;
    Widget_setContainer(self->contentWidget, 0);
    Widget_setContainer(self->buttonWidget, 0);
    PSC_Event_unregister(Widget_sizeRequested(self->buttonWidget), self,
	    sizeRequested, 0);
    PSC_Event_unregister(Widget_sizeRequested(self->contentWidget), self,
	    sizeRequested, 0);
    Object_destroy(self->contentWidget);
    Object_destroy(self->buttonWidget);
    free(self);
}

void TabBox_addTab(void *self, void *buttonWidget, void *contentWidget)
{
    TabBox *b = Object_instance(self);
    Tab *tab = PSC_malloc(sizeof *tab);
    tab->box = b;
    tab->buttonWidget = Object_ref(Widget_cast(buttonWidget));
    tab->contentWidget = Object_ref(Widget_cast(contentWidget));
    tab->buttonMinSize = Widget_minSize(tab->buttonWidget);
    tab->contentMinSize = Widget_minSize(tab->contentWidget);
    tab->index = PSC_List_size(b->tabs);
    PSC_Event_register(Widget_sizeRequested(tab->buttonWidget), tab,
	    sizeRequested, 0);
    PSC_Event_register(Widget_sizeRequested(tab->contentWidget), tab,
	    sizeRequested, 0);
    Font *font = Widget_font(b);
    Widget_setContainer(tab->buttonWidget, b);
    if (font) Widget_offerFont(tab->buttonWidget, font);
    Widget_setCursor(tab->buttonWidget, XC_HAND);
    Widget_setContainer(tab->contentWidget, b);
    if (font) Widget_offerFont(tab->contentWidget, font);
    PSC_List_append(b->tabs, tab, destroyTab);
    if (b->currentIndex < 0) b->currentIndex = 0;
    layout(b);
}

void TabBox_setTab(void *self, int index)
{
    TabBox *b = Object_instance(self);
    size_t ntabs = PSC_List_size(b->tabs);
    if (!ntabs) index = -1;
    else if (index < 0) index = 0;
    else if ((size_t)index >= ntabs) index = ntabs - 1;

    if (index != b->currentIndex)
    {
	b->currentIndex = index;
	layout(b);
    }
}

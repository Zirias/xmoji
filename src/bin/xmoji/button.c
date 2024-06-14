#include "button.h"

#include "font.h"
#include "textlabel.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static void enter(void *obj);
static void leave(void *obj);
static void setFont(void *obj, Font *font);
static int clicked(void *obj, const ClickEvent *event);
static Size minSize(const void *obj);

static MetaButton mo = MetaButton_init(
	0, draw, 0, 0,
	0, 0, enter, leave, 0, 0, 0, 0, setFont,
	0, minSize, 0, clicked, 0,
	"Button", destroy);

struct Button
{
    Object base;
    TextLabel *label;
    PSC_Event *clicked;
    ColorRole color;
};

static void destroy(void *obj)
{
    Button *self = obj;
    PSC_Event_destroy(self->clicked);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    Button *self = Object_instance(obj);
    if (picture)
    {
	Widget_invalidate(self->label);
	Rect geom = Widget_geometry(self->label);
	xcb_rectangle_t rect = { geom.pos.x, geom.pos.y,
	    geom.size.width, geom.size.height };
	Color color = Widget_color(self, self->color);
	CHECK(xcb_render_fill_rectangles(X11Adapter_connection(),
		    XCB_RENDER_PICT_OP_OVER, picture, Color_xcb(color),
		    1, &rect),
		"Cannot draw button background on 0x%x", (unsigned)picture);
    }
    return Widget_draw(self->label);
}

static void enter(void *obj)
{
    Button *self = Object_instance(obj);
    if (self->color != COLOR_BG_ACTIVE)
    {
	self->color = COLOR_BG_ACTIVE;
	Widget_invalidate(self);
    }
}

static void leave(void *obj)
{
    Button *self = Object_instance(obj);
    if (self->color != COLOR_BG_ABOVE)
    {
	self->color = COLOR_BG_ABOVE;
	Widget_invalidate(self);
    }
}

static void setFont(void *obj, Font *font)
{
    const Button *self = Object_instance(obj);
    Widget_setFont(self->label, font);
}

static int clicked(void *obj, const ClickEvent *event)
{
    if (event->button == MB_LEFT)
    {
	Button *self = Object_instance(obj);
	PSC_Event_raise(self->clicked, 0, 0);
	return 1;
    }
    return 0;
}

static Size minSize(const void *obj)
{
    Button *self = Object_instance(obj);
    Size minSize = Widget_minSize(self->label);
    minSize.width += 2;
    minSize.height += 2;

    if (minSize.width < 120) minSize.width = 120;
    return minSize;
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Widget_requestSize(receiver);
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Button *self = receiver;
    SizeChangedEventArgs *ea = args;
    Size size = ea->newSize;
    size.width -= 2;
    size.height -= 2;
    Widget_setSize(self->label, size);
}

static void originChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Button *self = receiver;
    OriginChangedEventArgs *ea = args;
    Pos origin = ea->newOrigin;
    ++origin.x;
    ++origin.y;
    Widget_setOrigin(self->label, origin);
}

Button *Button_createBase(void *derived, const char *name, void *parent)
{
    REGTYPE(0);

    Button *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.type = OBJTYPE;
    self->base.base = Widget_createBase(derived, name, parent);
    self->label = TextLabel_create(0, self);
    self->clicked = PSC_Event_create(self);
    self->color = COLOR_BG_ABOVE;

    Widget_setBackground(self, 1, COLOR_BG_BELOW);
    Widget_setPadding(self, (Box){1, 1, 1, 1});
    Widget_setExpand(self, EXPAND_NONE);
    Widget_setCursor(self, XC_HAND);
    Widget_setContainer(self->label, self);
    Widget_setAlign(self->label, AH_CENTER|AV_MIDDLE);
    Widget_show(self->label);

    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);
    PSC_Event_register(Widget_originChanged(self), self, originChanged, 0);

    PSC_Event_register(Widget_sizeRequested(self->label), self,
	    sizeRequested, 0);

    return self;
}

PSC_Event *Button_clicked(void *self)
{
    Button *b = Object_instance(self);
    return b->clicked;
}

const UniStr *Button_text(const void *self)
{
    const Button *b = Object_instance(self);
    return TextLabel_text(b->label);
}

void Button_setText(void *self, const UniStr *text)
{
    Button *b = Object_instance(self);
    TextLabel_setText(b->label, text);
}


#include "button.h"

#include "command.h"
#include "font.h"
#include "textlabel.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static void enter(void *obj);
static void leave(void *obj);
static void setFont(void *obj, Font *font);
static int clicked(void *obj, const ClickEvent *event);
static Size minSize(const void *obj);

static MetaButton mo = MetaButton_init(
	expose, draw, 0, 0,
	0, 0, enter, leave, 0, 0, 0, 0, setFont,
	0, minSize, 0, clicked, 0,
	"Button", destroy);

struct Button
{
    Object base;
    TextLabel *label;
    PSC_Event *clicked;
    ColorRole color;
    ColorRole background;
    ColorRole hoverBackground;
    uint16_t minwidth;
    uint8_t borderwidth;
};

static void destroy(void *obj)
{
    Button *self = obj;
    PSC_Event_destroy(self->clicked);
    free(self);
}

static void expose(void *obj, Rect region)
{
    Button *self = Object_instance(obj);
    Widget_setAlign(self->label, Widget_align(self) | AV_MIDDLE);
    Widget_invalidateRegion(self->label, region);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    Button *self = Object_instance(obj);
    if (picture)
    {
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
    if (self->color != self->hoverBackground)
    {
	self->color = self->hoverBackground;
	Widget_invalidate(self);
    }
}

static void leave(void *obj)
{
    Button *self = Object_instance(obj);
    if (self->color != self->background)
    {
	self->color = self->background;
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
    minSize.width += 2 * self->borderwidth;
    minSize.height += 2 * self->borderwidth;

    if (minSize.width < self->minwidth) minSize.width = self->minwidth;
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
    size.width -= 2 * self->borderwidth;
    size.height -= 2 * self->borderwidth;
    Widget_setSize(self->label, size);
}

static void originChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Button *self = receiver;
    OriginChangedEventArgs *ea = args;
    Pos origin = ea->newOrigin;
    origin.x += self->borderwidth;
    origin.y += self->borderwidth;
    Widget_setOrigin(self->label, origin);
}

Button *Button_createBase(void *derived, const char *name, void *parent)
{
    Button *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->label = TextLabel_create(0, self);
    self->clicked = PSC_Event_create(self);
    self->color = COLOR_BG_ABOVE;
    self->background = COLOR_BG_ABOVE;
    self->hoverBackground = COLOR_BG_ACTIVE;
    self->minwidth = 120;
    self->borderwidth = 1;

    Widget_setBackground(self, 1, COLOR_BORDER);
    Widget_setPadding(self, (Box){1, 1, 1, 1});
    Widget_setExpand(self, EXPAND_NONE);
    Widget_setCursor(self, XC_HAND);
    Widget_setAlign(self, AH_CENTER|AV_MIDDLE);
    Widget_setContainer(self->label, self);
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

void Button_setBorderWidth(void *self, uint8_t width)
{
    Button *b = Object_instance(self);
    if (width == b->borderwidth) return;
    if (!b->borderwidth) Widget_setBackground(b, 1, COLOR_BORDER);
    else if (!width) Widget_setBackground(b, 0, 0);
    b->borderwidth = width;
    Pos origin = Widget_origin(b);
    origin.x += width;
    origin.y += width;
    Widget_setOrigin(b->label, origin);
    Widget_requestSize(self);
}

void Button_setColors(void *self, ColorRole normal, ColorRole hover)
{
    Button *b = Object_instance(self);
    b->color = normal;
    b->background = normal;
    b->hoverBackground = hover;
}

void Button_setLabelPadding(void *self, Box padding)
{
    Button *b = Object_instance(self);
    Widget_setPadding(b->label, padding);
}

void Button_setMinWidth(void *self, uint16_t width)
{
    Button *b = Object_instance(self);
    b->minwidth = width;
}

void Button_attachCommand(void *self, Command *command)
{
    Button *b = Object_instance(self);
    const UniStr *name = Command_name(command);
    const UniStr *description = Command_description(command);
    Command_attach(command, b, Button_clicked);
    if (name) TextLabel_setText(b->label, name);
    if (description) Widget_setTooltip(b, description, 0);
}


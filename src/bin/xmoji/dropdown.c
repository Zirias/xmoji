#include "dropdown.h"

#include "flyout.h"
#include "pen.h"
#include "shape.h"
#include "vbox.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static void setFont(void *obj, Font *font);
static Size minSize(const void *obj);
static int clicked(void *obj, const ClickEvent *event);

static MetaDropdown mo = MetaDropdown_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, setFont,
	0, minSize, 0, clicked, 0,
	"Dropdown", destroy);

struct Dropdown
{
    Object base;
    PSC_Event *selected;
    Pen *pen;
    Shape *arrow;
    VBox *box;
    Flyout *flyout;
    Size minSize;
    unsigned index;
};

static void destroy(void *obj)
{
    Dropdown *self = obj;
    Shape_destroy(self->arrow);
    Pen_destroy(self->pen);
    PSC_Event_destroy(self->selected);
    free(self);
}

static xcb_render_picture_t renderArrow(void *obj,
	xcb_render_picture_t ownerpic, const void *data)
{
    Dropdown *self = obj;
    const Size *sz = data;

    xcb_connection_t *c = X11Adapter_connection();
    xcb_screen_t *s = X11Adapter_screen();

    xcb_pixmap_t tmp = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 8, tmp, s->root, sz->height, sz->height),
	    "Cannot create arrow pixmap for 0x%x", (unsigned)ownerpic);
    uint32_t pictopts[] = {
	XCB_RENDER_POLY_MODE_IMPRECISE,
	XCB_RENDER_POLY_EDGE_SMOOTH
    };
    xcb_render_picture_t pic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, pic, tmp,
		X11Adapter_format(PICTFORMAT_ALPHA),
		XCB_RENDER_CP_POLY_MODE | XCB_RENDER_CP_POLY_EDGE, pictopts),
	    "Cannot create arrow picture for 0x%x", (unsigned)ownerpic);
    xcb_free_pixmap(c, tmp);
    Color color = 0;
    xcb_rectangle_t rect = {0, 0, sz->height, sz->height};
    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC,
		pic, Color_xcb(color), 1, &rect),
	    "Cannot clear arrow picture for 0x%x", (unsigned)ownerpic);
    if (!self->pen) self->pen = Pen_create();
    Pen_configure(self->pen, PICTFORMAT_ALPHA, 0xffffffff);
    xcb_render_triangle_t arr = {
	{ (sz->height << 14), (sz->height << 14) + (sz->height << 13) },
	{ (sz->height << 15) + (sz->height << 14),
	    (sz->height << 14) + (sz->height << 13) },
	{ (sz->height << 15), (sz->height << 15) + (sz->height << 13) }
    };
    CHECK(xcb_render_triangles(c, XCB_RENDER_PICT_OP_OVER,
		Pen_picture(self->pen, ownerpic), pic, 0, 0, 0, 1, &arr),
	    "Cannot render arrow for 0x%x", (unsigned)ownerpic);

    return pic;
}
static int draw(void *obj, xcb_render_picture_t picture)
{
    Dropdown *self = Object_instance(obj);
    int rc = 0;
    Object_bcall(rc, Widget, draw, self, picture);
    if (rc < 0 || !picture) goto done;
    if (!self->arrow)
    {
	self->arrow = Shape_create(renderArrow, sizeof self->minSize,
		&self->minSize);
	Shape_render(self->arrow, self, picture);
    }
    if (!self->pen) self->pen = Pen_create();
    Pen_configure(self->pen, PICTFORMAT_RGB, Widget_color(self, COLOR_NORMAL));
    Pos origin = Widget_origin(self);
    CHECK(xcb_render_composite(X11Adapter_connection(),
		XCB_RENDER_PICT_OP_OVER, Pen_picture(self->pen, picture),
		Shape_picture(self->arrow), picture, 0, 0, 0, 0,
		origin.x + self->minSize.width - self->minSize.height,
		origin.y, self->minSize.height, self->minSize.height),
	    "Cannot composite arrow for 0x%x", (unsigned)picture);
done:
    return rc;
}

static void setFont(void *obj, Font *font)
{
    Dropdown *self = Object_instance(obj);
    Object_bcallv(Widget, setFont, self, font);
    if (self->box) Widget_setFont(self->box, font);
}

static Size minSize(const void *obj)
{
    const Dropdown *self = Object_instance(obj);
    return self->minSize;
}

static int clicked(void *obj, const ClickEvent *event)
{
    Dropdown *self = Object_instance(obj);
    if (self->flyout && event->button == MB_LEFT)
    {
	Flyout_popup(self->flyout, self);
	return 1;
    }
    return 0;
}

Dropdown *Dropdown_createBase(void *derived, const char *name, void *parent)
{
    Dropdown *self = PSC_malloc(sizeof *self);
    CREATEBASE(Button, name, parent);
    self->selected = PSC_Event_create(self);
    self->pen = 0;
    self->arrow = 0;
    self->box = 0;
    self->flyout = 0;
    self->minSize = (Size){0, 0};
    self->index = 0;

    Button_setLabelAlign(self, AV_MIDDLE);
    Widget_setPadding(self, (Box){0, 0, 0, 0});
    Widget_setExpand(self, EXPAND_X);

    return self;
}

static void itemClicked(void *receiver, void *sender, void *args)
{
    (void)args;

    Dropdown *self = receiver;
    int index = VBox_indexOf(self->box, sender);
    if (index >= 0 && (unsigned)index != self->index)
    {
	self->index = index;
	Button_setText(self, Button_text(sender));
	Widget_invalidate(self);
	PSC_Event_raise(self->selected, 0, &self->index);
    }
}

static void optionSizeReq(void *receiver, void *sender, void *args)
{
    (void)args;

    Dropdown *self = receiver;
    Size minSz = Widget_minSize(sender);
    minSz.width += minSz.height;

    int changed = 0;
    if (minSz.width > self->minSize.width)
    {
	self->minSize.width = minSz.width;
	++changed;
    }
    if (minSz.height > self->minSize.height)
    {
	self->minSize.height = minSz.height;
	++changed;
    }
    if (changed) Widget_requestSize(self);
}

void Dropdown_addOption(void *self, const UniStr *name)
{
    Dropdown *d = Object_instance(self);
    if (!d->flyout)
    {
	d->flyout = Flyout_create(0, d);
	Flyout_setIncBorder(d->flyout, 1);
	d->box = VBox_create(d);
	VBox_setSpacing(d->box, 0);
	Widget_setPadding(d->box, (Box){0, 0, 0, 0});
	Widget_setFont(d->box, Widget_font(d));
	Widget_show(d->box);
	Flyout_setWidget(d->flyout, d->box);
	Button_setText(d, name);
    }
    Button *b = Button_create(0, d->box);
    PSC_Event_register(Button_clicked(b), d, itemClicked, 0);
    Button_setText(b, name);
    Button_setBorderWidth(b, 0);
    Button_setLabelAlign(b, AV_MIDDLE);
    Button_setMinWidth(b, 0);
    Widget_setPadding(b, (Box){0, 0, 0, 0});
    Widget_setExpand(b, EXPAND_X|EXPAND_Y);
    Widget_show(b);
    VBox_addWidget(d->box, b);
    PSC_Event_register(Widget_sizeRequested(b), d, optionSizeReq, 0);
    optionSizeReq(d, b, 0);
}

unsigned Dropdown_selectedIndex(const void *self)
{
    const Dropdown *d = Object_instance(self);
    return d->index;
}

PSC_Event *Dropdown_selected(void *self)
{
    Dropdown *d = Object_instance(self);
    return d->selected;
}

void Dropdown_select(void *self, unsigned index)
{
    Dropdown *d = Object_instance(self);
    if (index == d->index) return;
    if (index >= VBox_rows(d->box)) return;
    d->index = index;
    Button_setText(self, Button_text(VBox_widget(d->box, index)));
    Widget_invalidate(self);
}


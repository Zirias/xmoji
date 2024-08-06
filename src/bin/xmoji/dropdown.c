#include "dropdown.h"

#include "flyout.h"
#include "vbox.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static int clicked(void *obj, const ClickEvent *event);

static MetaDropdown mo = MetaDropdown_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, minSize, 0, clicked, 0,
	"Dropdown", destroy);

struct Dropdown
{
    Object base;
    PSC_Event *selected;
    VBox *box;
    Flyout *flyout;
    Size minSize;
    unsigned index;
};

static void destroy(void *obj)
{
    Dropdown *self = obj;
    PSC_Event_destroy(self->selected);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    Dropdown *self = Object_instance(obj);
    int rc = 0;
    Object_bcall(rc, Widget, draw, self, picture);
    return rc;
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
    Widget_setFont(b, Widget_font(d));
    Widget_show(b);
    VBox_addWidget(d->box, b);
    Size minSz = Widget_minSize(b);
    minSz.width += minSz.height;
    if (minSz.width > d->minSize.width || minSz.height > d->minSize.height)
    {
	d->minSize = minSz;
	Widget_requestSize(d);
    }
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


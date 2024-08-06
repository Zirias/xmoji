#include "flyout.h"

#include "window.h"

#include <poser/core.h>
#include <stdlib.h>

C_CLASS_DECL(Font);

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static int show(void *obj);
static int hide(void *obj);
static Size minSize(const void *obj);
static void leave(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaFlyout mo = MetaFlyout_init(
	expose, draw, show, hide,
	0, 0, 0, leave, 0, 0, 0, 0, setFont, childAt,
	minSize, 0, clicked, 0,
	"Flyout", destroy);

static unsigned refcnt;
static Window *window;

struct Flyout
{
    Object base;
    Widget *widget;
};

static void destroy(void *obj)
{
    Flyout *self = obj;
    Object_destroy(self->widget);
    if (!--refcnt) Object_destroy(window);
    free(self);
}

static void expose(void *obj, Rect region)
{
    Flyout *self = Object_instance(obj);
    if (self->widget)
    {
	Widget_invalidateRegion(self->widget, region);
    }
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    Flyout *self = Object_instance(obj);
    int rc = -1;
    if (self->widget)
    {
	rc = Widget_draw(self->widget);
    }
    return rc;
}

static int show(void *obj)
{
    Flyout *self = Object_instance(obj);
    if (!window) return -1;
    int rc = 0;
    Object_bcall(rc, Widget, show, self);
    return rc < 0 ? rc : Widget_show(window);
}

static int hide(void *obj)
{
    Flyout *self = Object_instance(obj);
    Window_close(window);
    int rc = 0;
    Object_bcall(rc, Widget, hide, self);
    return rc;
}

static Size minSize(const void *obj)
{
    const Flyout *self = Object_instance(obj);
    if (self->widget) return Widget_minSize(self->widget);
    return (Size){0, 0};
}

static void leave(void *obj)
{
    Flyout *self = Object_instance(obj);
    if (self->widget) Widget_leave(self->widget);
    Window_close(window);
}

static void setFont(void *obj, Font *font)
{
    Flyout *self = Object_instance(obj);
    if (self->widget)
    {
	Widget_offerFont(self->widget, font);
    }
}

static Widget *childAt(void *obj, Pos pos)
{
    Flyout *self = Object_instance(obj);
    if (self->widget) return Widget_enterAt(self->widget, pos);
    return Widget_cast(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    Flyout *self = Object_instance(obj);
    int handled = 0;
    if (self->widget) handled = Widget_clicked(self->widget, event);
    if (event->button == MB_LEFT)
    {
	Widget_hide(self);
	handled = 1;
    }
    return handled;
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    Flyout *self = receiver;
    SizeChangedEventArgs *ea = args;

    if (self->widget) Widget_setSize(self->widget, ea->newSize);
}

Flyout *Flyout_createBase(void *derived, const char *name, void *parent)
{
    Flyout *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->widget = 0;

    Widget_setPadding(self, (Box){0, 0, 0, 0});
    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Widget_requestSize(receiver);
}

void Flyout_setWidget(void *self, void *widget)
{
    Flyout *f = Object_instance(self);
    if (f->widget)
    {
	Widget_setContainer(f->widget, 0);
	PSC_Event_unregister(Widget_sizeRequested(f->widget), self,
		sizeRequested, 0);
	Object_destroy(f->widget);
    }
    if (widget)
    {
	f->widget = Object_ref(Widget_cast(widget));
	PSC_Event_register(Widget_sizeRequested(f->widget), self,
		sizeRequested, 0);
	Widget_setContainer(f->widget, f);
    }
    else f->widget = 0;
}

void Flyout_popup(void *self, void *widget)
{
    Flyout *f = Object_instance(self);
    if (!f->widget) return;
    if (!window)
    {
	window = Window_create(0,
		WF_WINDOW_MENU|WF_POS_PARENTWIDGET|WF_ALWAYS_CLASS, 0);
    }
    Widget_setContainer(window, widget);
    Window_setMainWidget(window, f);
    Widget_show(f);
}

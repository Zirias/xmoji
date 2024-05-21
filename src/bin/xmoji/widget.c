#include "widget.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);
static int draw(void *obj);
static int show(void *obj);
static int hide(void *obj);
static Size minSize(const void *obj);

static MetaWidget mo = MetaWidget_init("Widget",
	destroy, draw, show, hide, minSize);

struct Widget
{
    Object base;
    PSC_Event *shown;
    PSC_Event *hidden;
    PSC_Event *sizeRequested;
    PSC_Event *sizeChanged;
    Size size;
    int visible;
};

static void destroy(void *obj)
{
    Widget *self = obj;
    PSC_Event_destroy(self->sizeChanged);
    PSC_Event_destroy(self->sizeRequested);
    PSC_Event_destroy(self->hidden);
    PSC_Event_destroy(self->shown);
    free(self);
}

static int draw(void *obj)
{
    (void)obj;
    return 0;
}

static int show(void *obj)
{
    Widget *self = obj;
    if (!self->visible)
    {
	self->visible = 1;
	PSC_Event_raise(self->shown, 0, 0);
    }
    return 0;
}

static int hide(void *obj)
{
    Widget *self = obj;
    if (self->visible)
    {
	self->visible = 0;
	PSC_Event_raise(self->hidden, 0, 0);
    }
    return 0;
}

static Size minSize(const void *obj)
{
    (void)obj;
    return (Size){0, 0};
}

Widget *Widget_create(void *parent)
{
    (void)parent;

    REGTYPE(0);

    Widget *self = PSC_malloc(sizeof *self);
    self->base.base = 0;
    self->base.type = mo.base.id;
    self->shown = PSC_Event_create(self);
    self->hidden = PSC_Event_create(self);
    self->sizeRequested = PSC_Event_create(self);
    self->sizeChanged = PSC_Event_create(self);
    self->size.width = 0;
    self->size.height = 0;
    self->visible = 0;

    return self;
}

PSC_Event *Widget_shown(void *self)
{
    Widget *w = Object_instance(self);
    return w->shown;
}

PSC_Event *Widget_hidden(void *self)
{
    Widget *w = Object_instance(self);
    return w->hidden;
}

PSC_Event *Widget_sizeRequested(void *self)
{
    Widget *w = Object_instance(self);
    return w->sizeRequested;
}

PSC_Event *Widget_sizeChanged(void *self)
{
    Widget *w = Object_instance(self);
    return w->sizeChanged;
}

int Widget_draw(void *self)
{
    int rc = -1;
    Object_vcall(rc, Widget, draw, self);
    return rc;
}

int Widget_show(void *self)
{
    int rc = -1;
    Object_vcall(rc, Widget, show, self);
    return rc;
}

int Widget_hide(void *self)
{
    int rc = -1;
    Object_vcall(rc, Widget, hide, self);
    return rc;
}

void Widget_setSize(void *self, Size size)
{
    Widget *w = Object_instance(self);
    if (memcmp(&w->size, &size, sizeof size))
    {
	SizeChangedEventArgs args = { w->size, size };
	w->size = size;
	PSC_Event_raise(w->sizeChanged, 0, &args);
    }
}

Size Widget_minSize(const void *self)
{
    Size size = { 0, 0 };
    Object_vcall(size, Widget, minSize, self);
    return size;
}

Size Widget_size(const void *self)
{
    Widget *w = Object_instance(self);
    return w->size;
}

int Widget_visible(const void *self)
{
    Widget *w = Object_instance(self);
    return w->visible;
}


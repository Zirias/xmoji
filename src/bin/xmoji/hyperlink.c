#include "hyperlink.h"

#include "unistr.h"
#include "xdgopen.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static void enter(void *obj);
static void leave(void *obj);
static int clicked(void *obj, const ClickEvent *event);

static MetaHyperLink mo = MetaHyperLink_init(
	0, 0, 0, 0, 0, 0, enter, leave, 0, 0, 0, 0, 0,
	0, 0, 0, clicked, 0,
	"HyperLink", destroy);

struct HyperLink
{
    Object base;
    char *link;
};

static void destroy(void *obj)
{
    HyperLink *self = obj;
    free(self->link);
    free(self);
}

static void enter(void *obj)
{
    TextLabel_setColor(obj, COLOR_HOVER);
}

static void leave(void *obj)
{
    TextLabel_setColor(obj, COLOR_LINK);
}

static int clicked(void *obj, const ClickEvent *event)
{
    if (event->button != MB_LEFT) return 0;
    HyperLink *self = Object_instance(obj);
    if (!self->link) return 0;
    xdgOpen(self->link);
    return 1;
}

HyperLink *HyperLink_createBase(void *derived, const char *name, void *parent)
{
    HyperLink *self = PSC_malloc(sizeof *self);
    CREATEBASE(TextLabel, name, parent);
    self->link = 0;
    TextLabel_setColor(self, COLOR_LINK);
    TextLabel_setUnderline(self, 2);
    Widget_setCursor(self, XC_HAND);
    return self;
}

const char *HyperLink_link(const void *self)
{
    const HyperLink *l = Object_instance(self);
    return l->link;
}

void HyperLink_setLink(void *self, const char *link)
{
    HyperLink *l = Object_instance(self);
    free(l->link);
    if (link)
    {
	l->link = PSC_copystr(link);
	UniStr *tt = UniStr_create(link);
	Widget_setTooltip(l, tt, 0);
	UniStr_destroy(tt);
    }
    else
    {
	l->link = 0;
	Widget_setTooltip(l, 0, 0);
    }
}


#include "spinbox.h"

#include "textbox.h"
#include "unistr.h"

#include <errno.h>
#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static void expose(void *obj, Rect region);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void leave(void *obj);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static int clicked(void *obj, const ClickEvent *event);

static MetaSpinBox mo = MetaSpinBox_init(
	expose, draw, 0, 0,
	0, 0, 0, leave, 0, 0, 0, unselect, setFont, childAt,
	minSize, 0, clicked, 0,
	"SpinBox", destroy);

struct SpinBox
{
    Object base;
    TextBox *textBox;
    PSC_Event *changed;
    unsigned minlen;
    int val;
    int min;
    int max;
    int step;
};

static void destroy(void *obj)
{
    SpinBox *self = obj;
    PSC_Event_destroy(self->changed);
    free(self);
}

static void expose(void *obj, Rect region)
{
    SpinBox *self = Object_instance(obj);
    Widget_invalidateRegion(self->textBox, region);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    (void)picture;

    SpinBox *self = Object_instance(obj);
    return Widget_draw(self->textBox);
}

static Size minSize(const void *obj)
{
    const SpinBox *self = Object_instance(obj);
    return Widget_minSize(self->textBox);
}

static void leave(void *obj)
{
    SpinBox *self = Object_instance(obj);
    Widget_leave(self->textBox);
}

static void unselect(void *obj)
{
    SpinBox *self = Object_instance(obj);
    Widget_unselect(self->textBox);
}

static void setFont(void *obj, Font *font)
{
    SpinBox *self = Object_instance(obj);
    Widget_offerFont(self->textBox, font);
}

static Widget *childAt(void *obj, Pos pos)
{
    (void)pos;

    SpinBox *self = Object_instance(obj);
    return Widget_cast(self->textBox);
}

static int clicked(void *obj, const ClickEvent *event)
{
    SpinBox *self = Object_instance(obj);
    return Widget_clicked(self->textBox, event);
}

static void layoutChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    SpinBox *self = receiver;
    Rect geom = Widget_geometry(self);
    Widget_setSize(self->textBox, geom.size);
    Widget_setOrigin(self->textBox, geom.pos);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    SpinBox *self = receiver;
    Widget_requestSize(self);
}

static int tryParse(int *value, int min, int max, const UniStr *str)
{
    char *cstr = LATIN1(str);
    char *endp;
    errno = 0;
    int ok = 0;
    long lval = strtol(cstr, &endp, 10);
    if (*endp || endp == cstr || errno != 0) goto done;
    if (lval < min || lval > max) goto done;
    if (value) *value = lval;
    ok = 1;
done:
    free(cstr);
    return ok;
}

static UniStr *valstr(int value)
{
    char buf[64];
    snprintf(buf, sizeof buf, "%d", value);
    return UniStr_create(buf);
}

static int filter(void *obj, const UniStr *str)
{
    SpinBox *self = obj;

    if (UniStr_len(str) <= self->minlen)
    {
	int ok = 1;
	const char32_t *s = UniStr_str(str);
	for (unsigned i = 0; i < UniStr_len(str); ++i)
	{
	    if (self->min < 0 && !i && *s == U'-') continue;
	    if (s[i] < U'0' || s[i] > U'9')
	    {
		ok = 0;
		break;
	    }
	}
	return ok;
    }

    return tryParse(0, self->min, self->max, str);
}

SpinBox *SpinBox_createBase(void *derived, const char *name,
	int min, int max, int step, void *parent)
{
    SpinBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->textBox = TextBox_create(name, self);
    self->changed = PSC_Event_create(self);
    UniStr *minstr = valstr(min);
    UniStr *maxstr = valstr(max);
    self->minlen = UniStr_len(minstr);
    if (UniStr_len(maxstr) < self->minlen) self->minlen = UniStr_len(maxstr);
    self->val = min;
    self->min = min;
    self->max = max;
    self->step = step;

    TextBox_setText(self->textBox, minstr);
    UniStr_destroy(maxstr);
    UniStr_destroy(minstr);

    TextBox_setInputFilter(self->textBox, self, filter);
    Widget_setContainer(self->textBox, self);
    Widget_show(self->textBox);
    PSC_Event_register(Widget_sizeRequested(self->textBox), self,
	    sizeRequested, 0);

    Widget_setPadding(self, (Box){0, 0, 0, 0});
    PSC_Event_register(Widget_sizeChanged(self), self, layoutChanged, 0);
    PSC_Event_register(Widget_originChanged(self), self, layoutChanged, 0);

    return self;
}

int SpinBox_value(const void *self)
{
    const SpinBox *b = Object_instance(self);
    return b->min;
}

PSC_Event *SpinBox_valueChanged(void *self)
{
    SpinBox *b = Object_instance(self);
    return b->changed;
}

void SpinBox_setValue(void *self, int value)
{
    (void)self;
    (void)value;
}


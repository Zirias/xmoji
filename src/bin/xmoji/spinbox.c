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

typedef enum SBHover
{
    SBH_NONE,
    SBH_TEXT,
    SBH_UP,
    SBH_DOWN
} SBHover;

struct SpinBox
{
    Object base;
    TextBox *textBox;
    PSC_Event *changed;
    Size minSize;
    unsigned minlen;
    int val;
    int min;
    int max;
    int step;
    int inval;
    SBHover hover;
};

static void destroy(void *obj)
{
    SpinBox *self = obj;
    PSC_Event_destroy(self->changed);
    free(self);
}

static UniStr *valstr(int value)
{
    char buf[64];
    snprintf(buf, sizeof buf, "%d", value);
    return UniStr_create(buf);
}

static void expose(void *obj, Rect region)
{
    SpinBox *self = Object_instance(obj);
    Widget_invalidateRegion(self->textBox, region);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    SpinBox *self = Object_instance(obj);
    int rc = Widget_draw(self->textBox);
    if (rc == 0 && picture)
    {
	xcb_connection_t *c = X11Adapter_connection();
	Rect geom = Widget_geometry(self->textBox);
	geom.pos.x += geom.size.width;
	geom.size.width = geom.size.height / 2;
	xcb_rectangle_t uprect = {
	    geom.pos.x + 1, geom.pos.y + 1,
	    geom.size.width - 2, geom.size.width - 1 };
	Color upcol = Widget_color(self,
		self->hover == SBH_UP ? COLOR_BG_ACTIVE : COLOR_BG_ABOVE);
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    picture, Color_xcb(upcol), 1, &uprect),
		"Cannot draw up button background for 0x%x",
		(unsigned)picture);
	xcb_rectangle_t downrect = {
	    geom.pos.x + 1, geom.pos.y + geom.size.width + 1,
	    geom.size.width - 2, geom.size.width - 1
		+ !(geom.size.height & 1)};
	Color downcol = Widget_color(self,
		self->hover == SBH_DOWN ? COLOR_BG_ACTIVE : COLOR_BG_ABOVE);
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    picture, Color_xcb(downcol), 1, &downrect),
		"Cannot draw down button background for 0x%x",
		(unsigned)picture);
    }
    return rc;
}

static Size minSize(const void *obj)
{
    const SpinBox *self = Object_instance(obj);
    return self->minSize;
}

static void leavetb(SpinBox *self)
{
    if (self->inval)
    {
	UniStr *str = valstr(self->val);
	TextBox_setText(self->textBox, str);
	UniStr_destroy(str);
	self->inval = 0;
    }
    Widget_leave(self->textBox);
}

static void leave(void *obj)
{
    SpinBox *self = Object_instance(obj);
    leavetb(self);
    if (self->hover != SBH_NONE)
    {
	self->hover = SBH_NONE;
	Rect geom = Widget_geometry(self->textBox);
	geom.pos.x += geom.size.width;
	geom.size.width = geom.size.height / 2;
	Widget_invalidateRegion(self, geom);
    }
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
    Widget *child = Widget_cast(self);
    SBHover hover;
    Rect textgeom = Widget_geometry(self->textBox);
    if (Rect_containsPos(textgeom, pos))
    {
	hover = SBH_TEXT;
	child = Widget_enterAt(self->textBox, pos);
    }
    else
    {
	leavetb(self);
	if (pos.y > textgeom.pos.y + textgeom.size.height / 2)
	{
	    hover = SBH_DOWN;
	}
	else hover = SBH_UP;
    }
    if (hover != self->hover)
    {
	self->hover = hover;
	textgeom.pos.x += textgeom.size.width;
	textgeom.size.width = textgeom.size.height / 2;
	Widget_invalidateRegion(self, textgeom);
    }
    return child;
}

static int clicked(void *obj, const ClickEvent *event)
{
    SpinBox *self = Object_instance(obj);
    Rect geom = Widget_geometry(self->textBox);
    if (Rect_containsPos(geom, event->pos))
    {
	return Widget_clicked(self->textBox, event);
    }
    if (event->button != MB_LEFT) return 0;
    geom.pos.x += geom.size.width;
    geom.size.width = geom.size.height / 2;
    geom.size.height /= 2;
    int value = self->val;
    int step = self->step;
    if (Rect_containsPos(geom, event->pos))
    {
	if (self->max - self->val < step) step = self->max - self->val;
	value += step;
    }
    else
    {
	if (self->val - self->min < step) step = self->val - self->min;
	value -= step;
    }
    if (value != self->val)
    {
	self->val = value;
	UniStr *str = valstr(value);
	TextBox_setText(self->textBox, str);
	UniStr_destroy(str);
	PSC_Event_raise(self->changed, 0, &self->val);
    }
    return 1;
}

static void layoutChanged(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    SpinBox *self = receiver;
    Rect geom = Widget_geometry(self);
    if (geom.size.width > geom.size.height / 2)
    {
	geom.size.width -= geom.size.height / 2;
    }
    else geom.size.width = 0;
    Widget_setSize(self->textBox, geom.size);
    Widget_setOrigin(self->textBox, geom.pos);
}

static void sizeRequested(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    SpinBox *self = receiver;
    self->minSize = Widget_minSize(self->textBox);
    self->minSize.width += self->minSize.height / 2;
    Widget_requestSize(self);
}

static int tryParse(int *value, int min, int max, const UniStr *str)
{
    char *cstr = LATIN1(str);
    if (!cstr) return 0;
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

static void textChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    SpinBox *self = receiver;
    const UniStr *str = args;

    if (tryParse(&self->val, self->min, self->max, str))
    {
	self->inval = 0;
	PSC_Event_raise(self->changed, 0, &self->val);
    }
    else self->inval = 1;
}

SpinBox *SpinBox_createBase(void *derived, const char *name,
	int min, int max, int step, void *parent)
{
    SpinBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->textBox = TextBox_create(name, self);
    self->changed = PSC_Event_create(self);
    self->minSize = (Size){0, 0};
    UniStr *minstr = valstr(min);
    UniStr *maxstr = valstr(max);
    self->minlen = UniStr_len(minstr);
    if (UniStr_len(maxstr) < self->minlen) self->minlen = UniStr_len(maxstr);
    self->val = min;
    self->min = min;
    self->max = max;
    self->step = step;
    self->inval = 0;
    self->hover = SBH_NONE;

    TextBox_setText(self->textBox, minstr);
    UniStr_destroy(maxstr);
    UniStr_destroy(minstr);

    TextBox_setInputFilter(self->textBox, self, filter);
    Widget_setContainer(self->textBox, self);
    Widget_show(self->textBox);
    PSC_Event_register(Widget_sizeRequested(self->textBox), self,
	    sizeRequested, 0);
    PSC_Event_register(TextBox_textChanged(self->textBox), self,
	    textChanged, 0);

    Widget_setPadding(self, (Box){0, 0, 0, 0});
    Widget_setBackground(self, 1, COLOR_BG_BELOW);
    Widget_setCursor(self, XC_HAND);
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
    SpinBox *b = Object_instance(self);
    if (value > b->max) value = b->max;
    if (value < b->min) value = b->min;
    UniStr *str = valstr(value);
    TextBox_setText(b->textBox, str);
    UniStr_destroy(str);
    b->val = value;
}


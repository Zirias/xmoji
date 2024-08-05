#include "textbox.h"

#include "font.h"
#include "pen.h"
#include "shape.h"
#include "textrenderer.h"
#include "unistr.h"
#include "unistrbuilder.h"
#include "window.h"

#include <poser/core.h>
#include <stdlib.h>
#include <xkbcommon/xkbcommon-keysyms.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static int activate(void *obj);
static int deactivate(void *obj);
static void enter(void *obj);
static void leave(void *obj);
static void paste(void *obj, XSelectionContent content);
static void unselect(void *obj);
static void setFont(void *obj, Font *font);
static Widget *childAt(void *obj, Pos pos);
static Size minSize(const void *obj);
static void keyPressed(void *obj, const KeyEvent *event);
static int clicked(void *obj, const ClickEvent *event);
static void dragged(void *obj, const DragEvent *event);

static MetaTextBox mo = MetaTextBox_init(
	0, draw, 0, 0,
	activate, deactivate, enter, leave, 0, 0, paste, unselect, setFont,
	childAt, minSize, keyPressed, clicked, dragged,
	"TextBox", destroy);

struct TextBox
{
    Object base;
    InputFilter filter;
    void *filterobj;
    UniStrBuilder *text;
    TextRenderer *renderer;
    UniStr *phtext;
    UniStr *selected;
    TextRenderer *placeholder;
    Shape *clearbtn;
    Pen *pen;
    PSC_Event *textChanged;
    PSC_Timer *cursorBlink;
    Size minSize;
    Size clearbtnsz;
    Pos lastPos;
    Selection selection;
    unsigned maxlen;
    unsigned cursor;
    unsigned scrollpos;
    unsigned dragAnchor;
    int cursorvisible;
    int grab;
    int grabbed;
    int clear;
    int hoverclear;
};

static void updateSelected(TextBox *self)
{
    UniStr_destroy(self->selected);
    self->selected = 0;
    if (self->selection.len)
    {
	char32_t *substr = PSC_malloc(
		(self->selection.len + 1) * sizeof *substr);
	memcpy(substr, UniStr_str(UniStrBuilder_stringView(self->text))
		+ self->selection.start,
		self->selection.len * sizeof *substr);
	substr[self->selection.len] = 0;
	self->selected = UniStr_create(substr);
	Widget_setSelection(self, XSN_PRIMARY,
		(XSelectionContent){self->selected, XST_TEXT});
    }
}

void blink(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    TextBox *self = receiver;
    self->cursorvisible = !self->cursorvisible;
    Widget_invalidate(self);
}

static void destroy(void *obj)
{
    TextBox *self = obj;
    Pen_destroy(self->pen);
    Shape_destroy(self->clearbtn);
    PSC_Timer_destroy(self->cursorBlink);
    PSC_Event_destroy(self->textChanged);
    TextRenderer_destroy(self->placeholder);
    UniStr_destroy(self->phtext);
    UniStr_destroy(self->selected);
    TextRenderer_destroy(self->renderer);
    UniStrBuilder_destroy(self->text);
    free(self);
}

static xcb_render_picture_t renderClearbtn(void *obj,
	xcb_render_picture_t ownerpic, const void *data)
{
    TextBox *self = obj;
    const Size *sz = data;

    xcb_connection_t *c = X11Adapter_connection();
    xcb_screen_t *s = X11Adapter_screen();

    xcb_pixmap_t tmp = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 8, tmp, s->root, sz->width, sz->height),
	    "Cannot create clear button pixmap for 0x%x", (unsigned)ownerpic);
    xcb_render_picture_t pic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, pic, tmp,
		X11Adapter_format(PICTFORMAT_ALPHA), 0, 0),
	    "Cannot create clear button picture for 0x%x", (unsigned)ownerpic);
    xcb_free_pixmap(c, tmp);
    Color color = 0;
    xcb_rectangle_t rect = {0, 0, sz->width, sz->height};
    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC,
		pic, Color_xcb(color), 1, &rect),
	    "Cannot clear clear button picture for 0x%x", (unsigned)ownerpic);
    uint32_t w = sz->width << 16;
    uint32_t h = sz->height << 16;
    uint32_t off = sz->height << 14;
    uint32_t center = sz->height << 15;
    xcb_render_pointfix_t points[] = {
	{ off, center },
	{ center, off },
	{ center, h - off },
	{ w, off },
	{ w, h - off }
    };
    if (!self->pen) self->pen = Pen_create();
    Pen_configure(self->pen, PICTFORMAT_ALPHA, 0xffffffff);
    CHECK(xcb_render_tri_strip(c, XCB_RENDER_PICT_OP_OVER,
		Pen_picture(self->pen, ownerpic), pic, 0, 0, 0,
		sizeof points / sizeof *points, points),
	    "Cannot render clear button for 0x%x", (unsigned)ownerpic);
    uint32_t xoff = sz->height << 12;
    xcb_render_pointfix_t x1points[] = {
	{ center - xoff, off + xoff + xoff },
	{ center, off + xoff },
	{ w - off, h - off - xoff },
	{ w - off + xoff, h - off - xoff - xoff }
    };
    CHECK(xcb_render_tri_strip(c, XCB_RENDER_PICT_OP_OUT_REVERSE,
		Pen_picture(self->pen, ownerpic), pic, 0, 0, 0,
		sizeof x1points / sizeof *x1points, x1points),
	    "Cannot render clear glyph for 0x%x", (unsigned)ownerpic);
    xcb_render_pointfix_t x2points[] = {
	{ w - off + xoff, off + xoff + xoff },
	{ w - off, off + xoff },
	{ center, h - off - xoff },
	{ center - xoff, h - off - xoff - xoff }
    };
    CHECK(xcb_render_tri_strip(c, XCB_RENDER_PICT_OP_OUT_REVERSE,
		Pen_picture(self->pen, ownerpic), pic, 0, 0, 0,
		sizeof x2points / sizeof *x2points, x2points),
	    "Cannot render clear glyph for 0x%x", (unsigned)ownerpic);

    return pic;
}

static void prerender(TextBox *self, Size newSize)
{
    if (!self->clear) return;
    Size clearbtnsz = { newSize.height, newSize.height };
    if (clearbtnsz.width == self->clearbtnsz.width
	    && clearbtnsz.height == self->clearbtnsz.height) return;

    self->clearbtnsz = clearbtnsz;
    Shape *clearbtn = Shape_create(renderClearbtn, sizeof self->clearbtnsz,
	    &self->clearbtnsz);
    if (self->clearbtn) Shape_destroy(self->clearbtn);
    Shape_render(clearbtn, self, Widget_picture(self));
    self->clearbtn = clearbtn;
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    if (!picture) return 0;
    TextBox *self = Object_instance(obj);
    xcb_connection_t *c = X11Adapter_connection();
    Color color = Widget_color(self, COLOR_BELOW);
    Rect geom = Widget_geometry(self);
    Rect contentArea = Rect_pad(geom, Widget_padding(self));
    int rc = 0;
    unsigned cursorpos = 0;
    if (UniStr_len(UniStrBuilder_stringView(self->text)))
    {
	uint16_t maxx = contentArea.size.width;
	if (self->clear)
	{
	    if (!self->clearbtn) prerender(self, geom.size);
	    if (!self->pen) self->pen = Pen_create();
	    Pen_configure(self->pen, PICTFORMAT_RGB,
		    Widget_color(self, COLOR_NORMAL));
	    CHECK(xcb_render_composite(c, XCB_RENDER_PICT_OP_OVER,
			Pen_picture(self->pen, picture),
			Shape_picture(self->clearbtn), picture, 0, 0, 0, 0,
			geom.pos.x + geom.size.width - self->clearbtnsz.width,
			geom.pos.y,
			self->clearbtnsz.width, self->clearbtnsz.height),
		    "Cannot composite clear button on 0x%x",
		    (unsigned)picture);
	    maxx -= self->clearbtnsz.width;
	    Rect clip = geom;
	    clip.size.width = maxx;
	    Widget_addClip(self, clip);
	}
	Size textsz = TextRenderer_size(self->renderer);
	if (self->scrollpos > textsz.width ||
		textsz.width - self->scrollpos < maxx)
	{
	    if (maxx >= textsz.width) self->scrollpos = 0;
	    else self->scrollpos = textsz.width - maxx - 1;
	}
	cursorpos = TextRenderer_pixelOffset(self->renderer, self->cursor);
	if (cursorpos < self->scrollpos) self->scrollpos = cursorpos;
	else if (cursorpos >= maxx + self->scrollpos)
	{
	    self->scrollpos = cursorpos - maxx + 1;
	}
	contentArea.pos.x -= self->scrollpos;
	if (self->selection.len && Widget_active(self))
	{
	    Selection rendersel;
	    rendersel.start = TextRenderer_pixelOffset(
		    self->renderer, self->selection.start);
	    rendersel.len = TextRenderer_pixelOffset(self->renderer,
		    self->selection.start + self->selection.len)
		- rendersel.start;
	    Color selfgcol = Widget_color(self, COLOR_SELECTED);
	    Color selbgcol = Widget_color(self, COLOR_BG_SELECTED);
	    xcb_rectangle_t rect = { contentArea.pos.x + rendersel.start,
		contentArea.pos.y, rendersel.len, self->minSize.height };
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			picture, Color_xcb(selbgcol), 1, &rect),
		    "Cannot draw selection background on 0x%x",
		    (unsigned)picture);
	    rc = TextRenderer_renderWithSelection(self->renderer, picture,
		    color, contentArea.pos, rendersel, selfgcol);
	}
	else rc = TextRenderer_render(self->renderer,
		picture, color, contentArea.pos);
    }
    else
    {
	self->scrollpos = 0;
	if (self->placeholder && !Widget_focused(self))
	{
	    rc = TextRenderer_render(self->placeholder, picture,
		    Widget_color(self, COLOR_DISABLED), contentArea.pos);
	}
    }
    if (rc >= 0 && self->cursorvisible)
    {
	xcb_rectangle_t rect = { contentArea.pos.x + cursorpos,
	    contentArea.pos.y, 1, self->minSize.height };
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER, picture,
		    Color_xcb(color), 1, &rect),
		"Cannot draw cursor on 0x%x", (unsigned)picture);
    }
    return rc;
}

static void keyboardGrabbed(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    TextBox *self = obj;
    if (reply && !error)
    {
	self->grabbed = 1;
    }
    else Widget_deactivate(self);
}

static int activate(void *obj)
{
    TextBox *self = Object_instance(obj);
    PSC_Timer_start(self->cursorBlink);
    self->cursorvisible = 1;
    Widget_setBackground(self, 1, COLOR_BG_ACTIVE);
    Widget_invalidate(self);
    if (self->grab && !self->grabbed)
    {
	AWAIT(xcb_grab_keyboard(X11Adapter_connection(), 1,
		    Window_id(Window_fromWidget(self)), XCB_CURRENT_TIME,
		    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC),
		self, keyboardGrabbed);
    }
    return 1;
}

static int deactivate(void *obj)
{
    TextBox *self = Object_instance(obj);
    PSC_Timer_stop(self->cursorBlink);
    if (self->cursorvisible)
    {
	self->cursorvisible = 0;
    }
    Widget_setBackground(self, 1, COLOR_BG_BELOW);
    Widget_invalidate(self);
    if (self->grabbed)
    {
	CHECK(xcb_ungrab_keyboard(X11Adapter_connection(), XCB_CURRENT_TIME),
		"Cannot ungrab keyboard for 0x%x",
		(unsigned)Widget_picture(self));
	self->grabbed = 0;
    }
    return 1;
}

static void enter(void *obj)
{
    Widget_setBackground(obj, 1, COLOR_BG_ACTIVE);
    Widget_invalidate(obj);
}

static void leave(void *obj)
{
    TextBox *self = Object_instance(obj);
    if (self->grab) Widget_unfocus(self);
    if (!Widget_active(self))
    {
	Widget_setBackground(obj, 1, COLOR_BG_BELOW);
	Widget_invalidate(self);
    }
    if (self->clear) self->lastPos = (Pos){ -1, -1 };
}

static Size minSize(const void *obj)
{
    const TextBox *self = Object_instance(obj);
    return self->minSize;
}

static void updatehover(TextBox *self, Pos pos)
{
    const UniStr *str = UniStrBuilder_stringView(self->text);
    if (UniStr_len(str))
    {
	Rect geom = Widget_geometry(self);
	if (pos.x > geom.pos.x + geom.size.width - self->clearbtnsz.width)
	{
	    Widget_setCursor(self, XC_HAND);
	}
	else Widget_setCursor(self, XC_XTERM);
    }
    else Widget_setCursor(self, XC_XTERM);
    if (pos.x == self->lastPos.x && pos.y == self->lastPos.y)
    {
	Window *w = Window_fromWidget(self);
	if (w) Window_invalidateHover(w);
    }
    else self->lastPos = pos;
}

static void keyPressed(void *obj, const KeyEvent *event)
{
    TextBox *self = Object_instance(obj);
    const UniStr *str = UniStrBuilder_stringView(self->text);
    size_t len = UniStr_len(str);
    unsigned glen;
    Selection oldSelection = self->selection;

    switch (event->keysym)
    {
	case XKB_KEY_BackSpace:
	    if (self->selection.len)
	    {
		self->cursor = self->selection.start;
		UniStrBuilder_remove(self->text, self->cursor,
			self->selection.len);
		self->selection.len = 0;
		break;
	    }
	    if (!len || !self->cursor) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor);
	    self->cursor -= glen;
	    UniStrBuilder_remove(self->text, self->cursor, glen);
	    break;

	case XKB_KEY_Delete:
	    if (self->selection.len)
	    {
		self->cursor = self->selection.start;
		UniStrBuilder_remove(self->text, self->cursor,
			self->selection.len);
		self->selection.len = 0;
		break;
	    }
	    if (!len || self->cursor == len) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor + 1);
	    UniStrBuilder_remove(self->text, self->cursor, glen);
	    break;

	case XKB_KEY_Left:
	    if (!len) return;
	    if (self->cursor) glen = TextRenderer_glyphLen(
		    self->renderer, self->cursor);
	    else glen = 0;
	    if (event->modifiers & XM_SHIFT)
	    {
		if (!self->cursor) return;
		if (self->selection.len)
		{
		    if (self->cursor == self->selection.start)
		    {
			self->selection.start -= glen;
			self->selection.len += glen;
		    }
		    else self->selection.len -= glen;
		}
		else
		{
		    self->selection.len = glen;
		    self->selection.start = self->cursor - glen;
		}
	    }
	    else
	    {
		if (!self->selection.len && !self->cursor) return;
		self->selection.len = 0;
	    }
	    self->cursor -= glen;
	    goto cursoronly;

	case XKB_KEY_Right:
	    if (!len) return;
	    if (self->cursor < len) glen = TextRenderer_glyphLen(
		    self->renderer, self->cursor + 1);
	    else glen = 0;
	    if (event->modifiers & XM_SHIFT)
	    {
		if (self->cursor == len) return;
		if (self->selection.len)
		{
		    if (self->cursor == self->selection.start)
		    {
			self->selection.start += glen;
			self->selection.len -= glen;
		    }
		    else self->selection.len += glen;
		}
		else
		{
		    self->selection.len = glen;
		    self->selection.start = self->cursor;
		}
	    }
	    else
	    {
		if (!self->selection.len && self->cursor == len) return;
		self->selection.len = 0;
	    }
	    self->cursor += glen;
	    goto cursoronly;

	case XKB_KEY_Home:
	case XKB_KEY_Begin:
	    if (!len) return;
	    if (event->modifiers & XM_SHIFT)
	    {
		if (!self->cursor) return;
		if (self->selection.len)
		{
		    if (self->cursor == self->selection.start)
		    {
			self->selection.len += self->cursor;
		    }
		    else self->selection.len = self->selection.start;
		}
		else self->selection.len = self->cursor;
		self->selection.start = 0;
	    }
	    else
	    {
		if (!self->selection.len && !self->cursor) return;
		self->selection.len = 0;
	    }
	    self->cursor = 0;
	    goto cursoronly;

	case XKB_KEY_End:
	    if (!len) return;
	    if (event->modifiers & XM_SHIFT)
	    {
		if (self->cursor == len) return;
		if (self->selection.len)
		{
		    if (self->cursor == self->selection.start)
		    {
			self->selection.start += self->selection.len;
		    }
		}
		else self->selection.start = self->cursor;
		self->selection.len = len - self->selection.start;
	    }
	    else
	    {
		if (!self->selection.len && self->cursor == len) return;
		self->selection.len = 0;
	    }
	    self->cursor = len;
	    goto cursoronly;

	default:
	    if (event->codepoint < 0x20U) return;
	    UniStrBuilder *mod = self->text;
	    if (self->filter) mod = UniStrBuilder_clone(self->text);
	    unsigned cursor = self->cursor;

	    if (self->selection.len)
	    {
		cursor = self->selection.start;
		UniStrBuilder_remove(mod, cursor, self->selection.len);
	    }
	    if (len < self->maxlen)
	    {
		UniStrBuilder_insertChar(mod, cursor++, event->codepoint);
	    }
	    if (self->filter)
	    {
		if (self->filter(self->filterobj,
			    UniStrBuilder_stringView(mod)))
		{
		    UniStrBuilder_destroy(self->text);
		    self->text = mod;
		    str = UniStrBuilder_stringView(mod);
		}
		else
		{
		    UniStrBuilder_destroy(mod);
		    return;
		}
	    }
	    self->selection.len = 0;
	    self->cursor = cursor;
	    if (self->clear && !len) updatehover(self, self->lastPos);
	    break;
    }
    TextRenderer_setText(self->renderer, str);
    PSC_Event_raise(self->textChanged, 0, (void *)str);
cursoronly:
    self->cursorvisible = 1;
    Widget_invalidate(self);
    if (oldSelection.len != self->selection.len
	    || oldSelection.start != self->selection.start)
    {
	updateSelected(self);
    }
}

static void paste(void *obj, XSelectionContent content)
{
    if (content.type == XST_NONE) return;
    TextBox *self = Object_instance(obj);
    const UniStr *str = UniStrBuilder_stringView(self->text);
    size_t len = UniStr_len(str);
    if (len >= self->maxlen) return;
    size_t inslen = UniStr_len(content.data);
    if (inslen > self->maxlen - len) inslen = self->maxlen - len;
    UniStrBuilder *mod = self->text;
    if (self->filter) mod = UniStrBuilder_clone(self->text);
    UniStrBuilder_insertStr(mod, self->cursor,
	    UniStr_str(content.data), inslen);
    if (self->filter)
    {
	if (self->filter(self->filterobj, UniStrBuilder_stringView(mod)))
	{
	    UniStrBuilder_destroy(self->text);
	    self->text = mod;
	    str = UniStrBuilder_stringView(mod);
	}
	else
	{
	    UniStrBuilder_destroy(mod);
	    return;
	}
    }
    self->cursor += inslen;
    TextRenderer_setText(self->renderer, str);
    PSC_Event_raise(self->textChanged, 0, (void *)str);
    Widget_invalidate(self);
    if (self->clear && !len) updatehover(self, self->lastPos);
}

static void unselect(void *obj)
{
    TextBox *self = Object_instance(obj);
    if (!self->selected) return;
    self->selection.len = 0;
    UniStr_destroy(self->selected);
    self->selected = 0;
    if (Widget_active(self)) Widget_invalidate(self);
}

static void setFont(void *obj, Font *font)
{
    TextBox *self = Object_instance(obj);
    TextRenderer_setFont(self->renderer, font);
    TextRenderer_setText(self->renderer, UniStrBuilder_stringView(self->text));
    if (self->placeholder)
    {
	TextRenderer_setFont(self->placeholder, font);
	TextRenderer_setText(self->placeholder, self->phtext);
    }
    self->minSize.height = (Font_maxHeight(font) + 0x3f) >> 6;
    Widget_invalidate(self);
    Widget_requestSize(self);
}

static Widget *childAt(void *obj, Pos pos)
{
    TextBox *self = Object_instance(obj);
    Widget *child = Widget_cast(obj);
    if (self->clear) updatehover(self, pos);
    return child;
}

static int clicked(void *obj, const ClickEvent *event)
{
    TextBox *self = Object_instance(obj);
    if (event->button != MB_LEFT &&
	    !(event->button == MB_MIDDLE && Widget_active(self))) return 0;
    if (event->button == MB_LEFT && self->clear)
    {
	const UniStr *str = UniStrBuilder_stringView(self->text);
	if (UniStr_len(str))
	{
	    Rect geom = Widget_geometry(obj);
	    if (event->pos.x > geom.pos.x + geom.size.width
		    - self->clearbtnsz.width)
	    {
		self->selection.len = 0;
		self->selection.start = 0;
		UniStrBuilder_clear(self->text);
		updatehover(self, self->lastPos);
		Widget_invalidate(self);
		PSC_Event_raise(self->textChanged, 0, (void *)str);
		Widget_focus(self);
		return 1;
	    }
	}
    }
    if (event->button == MB_LEFT && !Widget_active(self))
    {
	Widget_focus(self);
	if (self->selected)
	{
	    Widget_setSelection(self, XSN_PRIMARY,
		    (XSelectionContent){self->selected, XST_TEXT});
	    return 1;
	}
    }
    unsigned len = UniStr_len(UniStrBuilder_stringView(self->text));
    unsigned index = self->cursor;
    unsigned selectlen = 0;
    Selection oldSelection = self->selection;
    if (event->button == MB_LEFT && event->dblclick)
    {
	index = len;
	selectlen = len;
    }
    else if (len)
    {
	Pos origin = Widget_contentOrigin(self, self->minSize);
	index = TextRenderer_charIndex(self->renderer,
		event->pos.x + self->scrollpos - origin.x);
	if (index > len) index = len;
    }
    if (index != self->cursor || self->selection.len != selectlen)
    {
	Widget_invalidate(self);
	self->cursorvisible = 1;
    }
    self->cursor = index;
    self->selection.len = selectlen;
    self->selection.start = 0;
    self->dragAnchor = -1;
    if (event->button == MB_MIDDLE) Widget_requestPaste(
	    self, XSN_PRIMARY, XST_TEXT);
    else if (oldSelection.len != self->selection.len
	    || oldSelection.start != self->selection.start)
    {
	updateSelected(self);
    }
    return 1;
}

static void dragged(void *obj, const DragEvent *event)
{
    TextBox *self = Object_instance(obj);
    if (event->button == MB_LEFT)
    {
	unsigned len = UniStr_len(UniStrBuilder_stringView(self->text));
	if (!len) return;
	Selection oldSelection = self->selection;
	Pos origin = Widget_contentOrigin(self, self->minSize);
	unsigned fromidx = self->dragAnchor;
	if (fromidx == (unsigned)-1)
	{
	    int frompos = event->from.x + self->scrollpos - origin.x;
	    fromidx = TextRenderer_charIndex(self->renderer,
		frompos > 0 ? frompos : 0);
	    if (fromidx > len) fromidx = len;
	    self->dragAnchor = fromidx;
	}
	int topos = event->to.x + self->scrollpos - origin.x;
	unsigned toidx = TextRenderer_charIndex(self->renderer,
		topos > 0 ? topos : 0);
	if (toidx > len) toidx = len;
	if (fromidx == toidx)
	{
	    if (fromidx == self->cursor) return;
	    self->cursor = fromidx;
	    self->selection.len = 0;
	    Widget_invalidate(self);
	    return;
	}
	unsigned selpos = fromidx;
	unsigned sellen;
	if (toidx < fromidx)
	{
	    selpos = toidx;
	    sellen = fromidx - toidx;
	}
	else sellen = toidx - fromidx;
	if (toidx != self->cursor
		|| selpos != self->selection.start
		|| sellen != self->selection.len)
	{
	    self->cursor = toidx;
	    self->selection.start = selpos;
	    self->selection.len = sellen;
	    Widget_invalidate(self);
	}
	if (oldSelection.len != self->selection.len
		|| oldSelection.start != self->selection.start)
	{
	    updateSelected(self);
	}
    }
}

TextBox *TextBox_createBase(void *derived, const char *name, void *parent)
{
    TextBox *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->filter = 0;
    self->filterobj = 0;
    self->text = UniStrBuilder_create();
    self->renderer = TextRenderer_create(self->base.base);
    TextRenderer_setNoLigatures(self->renderer, 1);
    self->cursorBlink = PSC_Timer_create();
    PSC_Timer_setMs(self->cursorBlink, 600);
    PSC_Event_register(PSC_Timer_expired(self->cursorBlink), self, blink, 0);
    self->phtext = 0;
    self->placeholder = 0;
    self->clearbtn = 0;
    self->pen = 0;
    self->textChanged = PSC_Event_create(self);
    self->selected = 0;
    self->minSize = (Size){ 120, 12 };
    self->clearbtnsz = (Size){ 0, 0 };
    self->lastPos = (Pos){ -1, -1 };
    self->selection = (Selection){ 0, 0 };
    self->maxlen = 128;
    self->cursor = 0;
    self->scrollpos = 0;
    self->cursorvisible = 0;
    self->grab = 0;
    self->grabbed = 0;
    self->clear = 0;
    self->hoverclear = 0;

    Widget_setBackground(self, 1, COLOR_BG_BELOW);
    Widget_setExpand(self, EXPAND_X);
    Widget_setCursor(self, XC_XTERM);
    Widget_acceptFocus(self, 1);

    return self;
}

const UniStr *TextBox_text(const void *self)
{
    const TextBox *b = Object_instance(self);
    return UniStrBuilder_stringView(b->text);
}

PSC_Event *TextBox_textChanged(void *self)
{
    TextBox *b = Object_instance(self);
    return b->textChanged;
}

void TextBox_setText(void *self, const UniStr *text)
{
    TextBox *b = Object_instance(self);
    UniStrBuilder_clear(b->text);
    UniStrBuilder_appendStr(b->text, UniStr_str(text));
    b->cursor = 0;
    b->selection.len = 0;
    b->selection.start = 0;
    TextRenderer_setText(b->renderer, text);
    Widget_invalidate(b);
}

unsigned TextBox_maxLen(const void *self)
{
    const TextBox *b = Object_instance(self);
    return b->maxlen;
}

void TextBox_setMaxLen(void *self, unsigned len)
{
    TextBox *b = Object_instance(self);
    b->maxlen = len;
}

void TextBox_setPlaceholder(void *self, const UniStr *text)
{
    TextBox *b = Object_instance(self);

    if (!text || !UniStr_len(text))
    {
	if (!b->placeholder) return;
	TextRenderer_destroy(b->placeholder);
	b->placeholder = 0;
	UniStr_destroy(b->phtext);
	b->phtext = 0;
    }
    else if (UniStr_equals(b->phtext, text)) return;
    if (b->placeholder) UniStr_destroy(b->phtext);
    else
    {
	b->placeholder = TextRenderer_create(b->base.base);
	Font *font = Widget_font(self);
	if (font) TextRenderer_setFont(b->placeholder, font);
    }
    b->phtext = UniStr_ref(text);
    TextRenderer_setText(b->placeholder, b->phtext);
    if (!UniStr_len(UniStrBuilder_stringView(b->text)))
    {
	Widget_invalidate(b);
    }
}

void TextBox_setGrab(void *self, int grab)
{
    TextBox *b = Object_instance(self);
    b->grab = grab;
}

void TextBox_setClearBtn(void *self, int enabled)
{
    TextBox *b = Object_instance(self);
    b->clear = enabled;
}

void TextBox_setInputFilter(void *self, void *obj, InputFilter filter)
{
    TextBox *b = Object_instance(self);
    b->filter = filter;
    b->filterobj = obj;
}


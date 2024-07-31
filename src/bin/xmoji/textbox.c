#include "textbox.h"

#include "font.h"
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
static Size minSize(const void *obj);
static void keyPressed(void *obj, const KeyEvent *event);
static int clicked(void *obj, const ClickEvent *event);
static void dragged(void *obj, const DragEvent *event);

static MetaTextBox mo = MetaTextBox_init(
	0, draw, 0, 0,
	activate, deactivate, enter, leave, 0, 0, paste, unselect, setFont,
	0, minSize, keyPressed, clicked, dragged,
	"TextBox", destroy);

struct TextBox
{
    Object base;
    UniStrBuilder *text;
    TextRenderer *renderer;
    UniStr *phtext;
    UniStr *selected;
    TextRenderer *placeholder;
    PSC_Event *textChanged;
    PSC_Timer *cursorBlink;
    Size minSize;
    Selection selection;
    unsigned maxlen;
    unsigned cursor;
    unsigned scrollpos;
    unsigned dragAnchor;
    int cursorvisible;
    int grab;
    int grabbed;
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
    PSC_Timer_destroy(self->cursorBlink);
    PSC_Event_destroy(self->textChanged);
    TextRenderer_destroy(self->placeholder);
    UniStr_destroy(self->phtext);
    UniStr_destroy(self->selected);
    TextRenderer_destroy(self->renderer);
    UniStrBuilder_destroy(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    if (!picture) return 0;
    TextBox *self = Object_instance(obj);
    xcb_connection_t *c = X11Adapter_connection();
    Color color = Widget_color(self, COLOR_BELOW);
    Rect contentArea = Rect_pad(Widget_geometry(self), Widget_padding(self));
    int rc = 0;
    unsigned cursorpos = 0;
    if (UniStr_len(UniStrBuilder_stringView(self->text)))
    {
	Size textsz = TextRenderer_size(self->renderer);
	if (self->scrollpos > textsz.width ||
		textsz.width - self->scrollpos < contentArea.size.width)
	{
	    if (contentArea.size.width >= textsz.width) self->scrollpos = 0;
	    else self->scrollpos = textsz.width - contentArea.size.width - 1;
	}
	cursorpos = TextRenderer_pixelOffset(self->renderer, self->cursor);
	if (cursorpos < self->scrollpos) self->scrollpos = cursorpos;
	else if (cursorpos >= contentArea.size.width + self->scrollpos)
	{
	    self->scrollpos = cursorpos - contentArea.size.width + 1;
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
}

static Size minSize(const void *obj)
{
    const TextBox *self = Object_instance(obj);
    return self->minSize;
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
	    if (self->selection.len)
	    {
		self->cursor = self->selection.start;
		UniStrBuilder_remove(self->text, self->cursor,
			self->selection.len);
		self->selection.len = 0;
	    }
	    if (len < self->maxlen)
	    {
		UniStrBuilder_insertChar(self->text,
			self->cursor++, event->codepoint);
	    }
	    break;
    }
    TextRenderer_setText(self->renderer, str);
    PSC_Event_raise(self->textChanged, 0, (void *)str);
cursoronly:
    PSC_Service_setTickInterval(600);
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
    UniStrBuilder_insertStr(self->text, self->cursor,
	    UniStr_str(content.data), inslen);
    self->cursor += inslen;
    TextRenderer_setText(self->renderer, str);
    PSC_Event_raise(self->textChanged, 0, (void *)str);
    Widget_invalidate(self);
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

static int clicked(void *obj, const ClickEvent *event)
{
    TextBox *self = Object_instance(obj);
    if (event->button != MB_LEFT &&
	    !(event->button == MB_MIDDLE && Widget_active(self))) return 0;
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
	PSC_Service_setTickInterval(600);
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
    self->text = UniStrBuilder_create();
    self->renderer = TextRenderer_create(self->base.base);
    TextRenderer_setNoLigatures(self->renderer, 1);
    self->cursorBlink = PSC_Timer_create();
    PSC_Timer_setMs(self->cursorBlink, 600);
    PSC_Event_register(PSC_Timer_expired(self->cursorBlink), self, blink, 0);
    self->phtext = 0;
    self->placeholder = 0;
    self->textChanged = PSC_Event_create(self);
    self->selected = 0;
    self->minSize = (Size){ 120, 12 };
    self->selection = (Selection){ 0, 0 };
    self->maxlen = 128;
    self->cursor = 0;
    self->scrollpos = 0;
    self->cursorvisible = 0;
    self->grab = 0;
    self->grabbed = 0;

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


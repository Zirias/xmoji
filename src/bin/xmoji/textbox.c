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
static Size minSize(const void *obj);
static void keyPressed(void *obj, const KeyEvent *event);
static void clicked(void *obj, const ClickEvent *click);

static MetaTextBox mo = MetaTextBox_init(
	0, draw, 0, 0,
	activate, deactivate, enter, leave, 0,
	minSize, keyPressed, clicked,
	"TextBox", destroy);

struct TextBox
{
    Object base;
    Font *font;
    UniStrBuilder *text;
    TextRenderer *renderer;
    UniStr *phtext;
    TextRenderer *placeholder;
    Size minSize;
    Selection selection;
    unsigned cursor;
    unsigned scrollpos;
    int cursorvisible;
};

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
    PSC_Event_unregister(PSC_Service_tick(), self, blink, 0);
    TextRenderer_destroy(self->placeholder);
    UniStr_destroy(self->phtext);
    TextRenderer_destroy(self->renderer);
    UniStrBuilder_destroy(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    if (!picture) return 0;
    TextBox *self = Object_instance(obj);
    xcb_connection_t *c = X11Adapter_connection();
    Color color = Widget_color(self, COLOR_NORMAL);
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
	if (self->selection.len)
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
	if (self->placeholder && !Widget_active(self))
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

static int activate(void *obj)
{
    TextBox *self = Object_instance(obj);
    PSC_Service_setTickInterval(600);
    PSC_Event_register(PSC_Service_tick(), self, blink, 0);
    self->cursorvisible = 1;
    Widget_setBackground(self, 1, COLOR_BG_ACTIVE);
    Widget_invalidate(self);
    return 1;
}

static int deactivate(void *obj)
{
    TextBox *self = Object_instance(obj);
    PSC_Event_unregister(PSC_Service_tick(), self, blink, 0);
    if (self->cursorvisible)
    {
	self->cursorvisible = 0;
    }
    Widget_setBackground(self, 1, COLOR_BG_NORMAL);
    Widget_invalidate(self);
    return 1;
}

static void enter(void *obj)
{
    Widget_setBackground(obj, 1, COLOR_BG_ACTIVE);
    Widget_invalidate(obj);
}

static void leave(void *obj)
{
    if (!Widget_active(obj))
    {
	Widget_setBackground(obj, 1, COLOR_BG_NORMAL);
	Widget_invalidate(obj);
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
	    if (!len || !self->cursor) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor);
	    if (event->modifiers & XM_SHIFT)
	    {
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
	    else self->selection.len = 0;
	    self->cursor -= glen;
	    goto cursoronly;

	case XKB_KEY_Right:
	    if (!len || self->cursor == len) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor + 1);
	    if (event->modifiers & XM_SHIFT)
	    {
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
	    else self->selection.len = 0;
	    self->cursor += glen;
	    goto cursoronly;

	case XKB_KEY_Home:
	case XKB_KEY_Begin:
	    if (!len || !self->cursor) return;
	    if (event->modifiers & XM_SHIFT)
	    {
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
	    else self->selection.len = 0;
	    self->cursor = 0;
	    goto cursoronly;

	case XKB_KEY_End:
	    if (!len || self->cursor == len) return;
	    if (event->modifiers & XM_SHIFT)
	    {
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
	    else self->selection.len = 0;
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
	    UniStrBuilder_insertChar(self->text,
		    self->cursor++, event->codepoint);
	    break;
    }
    TextRenderer_setText(self->renderer, str);
cursoronly:
    Widget_invalidate(self);
}

static void clicked(void *obj, const ClickEvent *event)
{
    TextBox *self = Object_instance(obj);
    if (event->button == MB_LEFT)
    {
	Window *w = Window_fromWidget(self);
	if (w) Window_setFocusWidget(w, self);
	unsigned len = UniStr_len(UniStrBuilder_stringView(self->text));
	unsigned index = self->cursor;
	if (len)
	{
	    Pos origin = Widget_contentOrigin(self, self->minSize);
	    index = TextRenderer_charIndex(self->renderer,
		    event->pos.x + self->scrollpos - origin.x);
	    if (index > len) index = len;
	}
	if (index != self->cursor || self->selection.len)
	{
	    Widget_invalidate(self);
	}
	self->cursor = index;
	self->selection.len = 0;
    }
}

TextBox *TextBox_createBase(void *derived, const char *name,
	void *parent, Font *font)
{
    REGTYPE(0);

    TextBox *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.base = Widget_createBase(derived, name, parent);
    self->base.type = OBJTYPE;
    self->font = font;
    self->text = UniStrBuilder_create();
    self->renderer = TextRenderer_create(font);
    TextRenderer_setNoLigatures(self->renderer, 1);
    self->phtext = 0;
    self->placeholder = 0;
    self->minSize = (Size){ 120, (Font_maxHeight(self->font) + 0x3f) >> 6 };
    self->selection = (Selection){ 0, 0 };
    self->cursor = 0;
    self->scrollpos = 0;
    self->cursorvisible = 0;

    Widget_setMaxSize(self, (Size){ -1, self->minSize.height });
    Widget_setBackground(self, 1, COLOR_BG_NORMAL);
    Widget_setCursor(self, XC_XTERM);

    return self;
}

const UniStr *TextBox_text(const void *self)
{
    TextBox *l = Object_instance(self);
    return UniStrBuilder_stringView(l->text);
}

void TextBox_setText(void *self, const UniStr *text)
{
    (void)self;
    (void)text;
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
    else b->placeholder = TextRenderer_create(b->font);
    b->phtext = UniStr_ref(text);
    TextRenderer_setText(b->placeholder, b->phtext);
    if (!UniStr_len(UniStrBuilder_stringView(b->text)))
    {
	Widget_invalidate(b);
    }
}


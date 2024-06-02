#include "textbox.h"

#include "font.h"
#include "textrenderer.h"
#include "unistr.h"
#include "unistrbuilder.h"

#include <poser/core.h>
#include <stdlib.h>
#include <xkbcommon/xkbcommon-keysyms.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);
static void keyPressed(void *obj, const KeyEvent *event);

static MetaTextBox mo = MetaTextBox_init("TextBox",
	destroy, draw, 0, 0, minSize, keyPressed);

struct TextBox
{
    Object base;
    Font *font;
    UniStrBuilder *text;
    TextRenderer *renderer;
    Size minSize;
    unsigned cursor;
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
    TextRenderer_destroy(self->renderer);
    UniStrBuilder_destroy(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    if (!picture) return 0;
    TextBox *self = Object_instance(obj);
    Color color = Widget_color(self, COLOR_NORMAL);
    Pos pos = Widget_contentOrigin(self, Widget_size(self));
    int rc = 0;
    unsigned cursorpos = 0;
    if (UniStr_len(UniStrBuilder_stringView(self->text)))
    {
	rc = TextRenderer_render(self->renderer, picture, color, pos);
	cursorpos = TextRenderer_pixelOffset(self->renderer, self->cursor);
    }
    if (rc >= 0 && self->cursorvisible)
    {
	xcb_rectangle_t rect = { pos.x + cursorpos, pos.y,
	    1, self->minSize.height };
	CHECK(xcb_render_fill_rectangles(X11Adapter_connection(),
		    XCB_RENDER_PICT_OP_OVER, picture, Color_xcb(color),
		    1, &rect),
		"Cannot draw cursor on 0x%x", (unsigned)picture);
    }
    return rc;
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
	    if (!len || !self->cursor) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor);
	    self->cursor -= glen;
	    UniStrBuilder_remove(self->text, self->cursor, glen);
	    break;

	case XKB_KEY_Delete:
	    if (!len || self->cursor == len) return;
	    glen = TextRenderer_glyphLen(self->renderer, self->cursor + 1);
	    UniStrBuilder_remove(self->text, self->cursor, glen);
	    break;

	case XKB_KEY_Left:
	    if (!len || !self->cursor) return;
	    self->cursor -= TextRenderer_glyphLen(self->renderer,
		    self->cursor);
	    goto cursoronly;

	case XKB_KEY_Right:
	    if (!len || self->cursor == len) return;
	    self->cursor += TextRenderer_glyphLen(self->renderer,
		    self->cursor + 1);
	    goto cursoronly;

	case XKB_KEY_Home:
	case XKB_KEY_Begin:
	    if (!len || !self->cursor) return;
	    self->cursor = 0;
	    goto cursoronly;

	case XKB_KEY_End:
	    if (!len || self->cursor == len) return;
	    self->cursor = len;
	    goto cursoronly;

	default:
	    if (event->codepoint < 0x20U) return;
	    UniStrBuilder_insertChar(self->text,
		    self->cursor++, event->codepoint);
	    break;
    }
    TextRenderer_setText(self->renderer, str);
cursoronly:
    Widget_invalidate(self);
}

TextBox *TextBox_createBase(void *derived, void *parent, Font *font)
{
    REGTYPE(0);

    TextBox *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.base = Widget_createBase(derived, parent, IE_KEYPRESSED);
    self->base.type = OBJTYPE;
    self->font = font;
    self->text = UniStrBuilder_create();
    self->renderer = TextRenderer_create(font);
    TextRenderer_setNoLigatures(self->renderer, 1);
    self->minSize = (Size){ 120, (Font_maxHeight(self->font) + 0x3f) >> 6 };
    self->cursor = 0;
    self->cursorvisible = 1;

    PSC_Service_setTickInterval(600);
    PSC_Event_register(PSC_Service_tick(), self, blink, 0);

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


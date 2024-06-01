#include "textbox.h"

#include "font.h"
#include "textrenderer.h"
#include "unistr.h"
#include "unistrbuilder.h"

#include <poser/core.h>
#include <stdlib.h>

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
};

static void destroy(void *obj)
{
    TextBox *self = obj;
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
    if (rc >= 0)
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
    if (!event->codepoint) return;
    TextBox *self = Object_instance(obj);
    UniStrBuilder_appendChar(self->text, event->codepoint);
    TextRenderer_setText(self->renderer,
	    UniStrBuilder_stringView(self->text));
    ++self->cursor;
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
    self->minSize = (Size){ 120, (Font_maxHeight(self->font) + 0x3f) >> 6 };
    self->cursor = 0;

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


#include "emojibutton.h"

#include "textlabel.h"
#include "textrenderer.h"
#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

static void unselect(void *obj);
static int clicked(void *obj, const ClickEvent *event);

static MetaEmojiButton mo = MetaEmojiButton_init(
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, unselect, 0,
	0, 0, 0, clicked, 0,
	"EmojiButton", free);

struct EmojiButton
{
    Object base;
    int selected;
};

static void renderCallback(void *ctx, TextRenderer *renderer)
{
    if (TextRenderer_nglyphs(renderer) != 1
	    || !TextRenderer_glyphIdAt(renderer, 0))
    {
	/* Try to strip variant selectors first in case the font has glyphs
	 * for emojis without these */
	char32_t stripped[16] = {0};
	unsigned strippedlen = 0;
	int trystripped = 0;
	EmojiButton *self = ctx;
	const UniStr *orig = Button_text(self);
	const char32_t *origstr = UniStr_str(orig);
	size_t origlen = UniStr_len(orig);
	if (origlen < 15) for (size_t i = 0; i < origlen; ++i)
	{
	    if (origstr[i] == 0xfe0e || origstr[i] == 0xfe0f)
	    {
		trystripped = 1;
	    }
	    else stripped[strippedlen++] = origstr[i];
	}
	if (trystripped)
	{
	    UniStr *newstr = UniStr_createFromUtf32(stripped);
	    TextRenderer_setText(renderer, newstr);
	    UniStr_destroy(newstr);
	    if (TextRenderer_nglyphs(renderer) == 1
		    && TextRenderer_glyphIdAt(renderer, 0)) return;
	}

	/* If this didn't help, use the default font and display the
	 * replacement character */
	Font *deffont = Font_create(0, 0);
	TextRenderer_setFont(renderer, deffont);
	UniStr(repl, U"\xfffd");
	TextRenderer_setText(renderer, repl);
	Font_destroy(deffont);
    }
}

static void unselect(void *obj)
{
    EmojiButton *self = Object_instance(obj);
    self->selected = 0;
    Button_setColors(self, COLOR_BG_NORMAL, COLOR_BG_ACTIVE);
    Widget_invalidate(self);
}

static int clicked(void *obj, const ClickEvent *event)
{
    EmojiButton *self = Object_instance(obj);
    if (event->button == MB_MIDDLE && !self->selected)
    {
	Widget_setSelection(self, XSN_PRIMARY,
		(XSelectionContent){(void *)Button_text(self), XST_TEXT});
	self->selected = 1;
	Button_setColors(self, COLOR_BG_SELECTED, COLOR_BG_SELECTED);
	Widget_invalidate(self);
	return 1;
    }
    int rc = 0;
    Object_bcall(rc, Widget, clicked, self, event);
    return rc;
}

EmojiButton *EmojiButton_createBase(void *derived,
	const char *name, void *parent)
{
    EmojiButton *self = PSC_malloc(sizeof *self);
    CREATEBASE(Button, name, parent);
    self->selected = 0;

    Button_setBorderWidth(self, 0);
    Button_setLabelPadding(self, (Box){0, 0, 0, 0});
    Button_setMinWidth(self, 0);
    Button_setColors(self, COLOR_BG_NORMAL, COLOR_BG_ACTIVE);
    Widget_setExpand(self, EXPAND_X|EXPAND_Y);
    Widget_setLocalUnselect(self, 1);
    TextLabel_setRenderCallback(Button_label(self), self, renderCallback);

    return self;
}

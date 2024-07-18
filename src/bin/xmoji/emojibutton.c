#include "emojibutton.h"

#include "flowgrid.h"
#include "flyout.h"
#include "pen.h"
#include "shape.h"
#include "textlabel.h"
#include "textrenderer.h"
#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static void unselect(void *obj);
static int clicked(void *obj, const ClickEvent *event);

static MetaEmojiButton mo = MetaEmojiButton_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, unselect, 0,
	0, 0, 0, clicked, 0,
	"EmojiButton", destroy);

struct EmojiButton
{
    Object base;
    FlowGrid *flowgrid;
    Flyout *flyout;
    Shape *triangle;
    Pen *pen;
    Size trianglesize;
    Color pencolor;
    int selected;
};

static void destroy(void *obj)
{
    EmojiButton *self = obj;
    Pen_destroy(self->pen);
    Shape_destroy(self->triangle);
    free(self);
}

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

static xcb_render_picture_t renderTriangle(void *obj,
	xcb_render_picture_t ownerpic, const void *data)
{
    EmojiButton *self = obj;
    const Size *sz = data;

    xcb_connection_t *c = X11Adapter_connection();
    xcb_screen_t *s = X11Adapter_screen();

    xcb_pixmap_t tmp = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 8, tmp, s->root, sz->width, sz->height),
	    "Cannot create triangle pixmap for 0x%x", (unsigned)ownerpic);
    xcb_render_picture_t pic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, pic, tmp,
		X11Adapter_format(PICTFORMAT_ALPHA), 0, 0),
	    "Cannot create triangle picture for 0x%x", (unsigned)ownerpic);
    xcb_free_pixmap(c, tmp);
    Color color = 0;
    xcb_rectangle_t rect = {0, 0, sz->width, sz->height};
    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC,
		pic, Color_xcb(color), 1, &rect),
	    "Cannot clear triangle picture for 0x%x", (unsigned)ownerpic);
    uint32_t x = sz->width << 16;
    uint32_t y = sz->height << 16;
    xcb_render_triangle_t triangle = {
	{ x - (sz->width << 14), y },
	{ x, y },
	{ x, y - (sz->height << 14) }
    };
    if (!self->pen) self->pen = Pen_create();
    Pen_configure(self->pen, PICTFORMAT_ALPHA, 0xffffffff);
    CHECK(xcb_render_triangles(c, XCB_RENDER_PICT_OP_OVER,
		Pen_picture(self->pen, ownerpic), pic, 0, 0, 0, 1, &triangle),
	"Cannot render triangle for 0x%x", (unsigned)ownerpic);

    return pic;
}

static void prerender(EmojiButton *self, Size newSize)
{
    if (self->flyout && newSize.width && newSize.height
	    && newSize.width != self->trianglesize.width
	    && newSize.height != self->trianglesize.height)
    {
	self->trianglesize = newSize;
	Shape *triangle = Shape_create(renderTriangle,
		sizeof self->trianglesize, &self->trianglesize);
	if (self->triangle) Shape_destroy(self->triangle);
	Shape_render(triangle, self, Widget_picture(self));
	self->triangle = triangle;
    }
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    EmojiButton *self = Object_instance(obj);
    int rc = 0;
    Object_bcall(rc, Widget, draw, self, picture);
    if (rc == 0 && picture && self->flyout)
    {
	xcb_connection_t *c = X11Adapter_connection();
	if (!self->pen) self->pen = Pen_create();
	Rect geom = Widget_geometry(self);
	if (!self->pencolor) prerender(self, geom.size);
	Color pencolor = Widget_color(self, COLOR_ACTIVE);
	if (pencolor != self->pencolor)
	{
	    Pen_configure(self->pen, PICTFORMAT_RGB, pencolor);
	    self->pencolor = pencolor;
	}
	CHECK(xcb_render_composite(c, XCB_RENDER_PICT_OP_OVER,
		    Pen_picture(self->pen, picture),
		    Shape_picture(self->triangle), picture, 0, 0, 0, 0,
		    geom.pos.x, geom.pos.y, geom.size.width, geom.size.height),
		"Cannot composite fly-out indicator on 0x%x",
		(unsigned)picture);
    }
    return rc;
}

static void unselect(void *obj)
{
    EmojiButton *self = Object_instance(obj);
    if (!self->selected) return;
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
    if (self->flyout && event->button == MB_RIGHT)
    {
	Widget_show(self->flowgrid);
	Flyout_popup(self->flyout, self);
	return 1;
    }
    int rc = 0;
    Object_bcall(rc, Widget, clicked, self, event);
    return rc;
}

static void sizeChanged(void *receiver, void *sender, void *args)
{
    (void)sender;

    EmojiButton *self = receiver;
    SizeChangedEventArgs *ea = args;

    prerender(self, ea->newSize);
}

EmojiButton *EmojiButton_createBase(void *derived,
	const char *name, void *parent)
{
    EmojiButton *self = PSC_malloc(sizeof *self);
    CREATEBASE(Button, name, parent);
    self->flowgrid = 0;
    self->flyout = 0;
    self->triangle = 0;
    self->pen = 0;
    self->trianglesize = (Size){0, 0};
    self->pencolor = 0;
    self->selected = 0;

    Button_setBorderWidth(self, 0);
    Button_setLabelPadding(self, (Box){1, 1, 1, 1});
    Button_setMinWidth(self, 0);
    Button_setColors(self, COLOR_BG_NORMAL, COLOR_BG_ACTIVE);
    Widget_setPadding(self, (Box){0, 0, 0, 0});
    Widget_setExpand(self, EXPAND_X|EXPAND_Y);
    Widget_setLocalUnselect(self, 1);
    TextLabel_setRenderCallback(Button_label(self), self, renderCallback);

    PSC_Event_register(Widget_sizeChanged(self), self, sizeChanged, 0);

    return self;
}

void EmojiButton_addVariant(void *self, EmojiButton *variant)
{
    EmojiButton *b = Object_instance(self);
    if (!b->flyout)
    {
	b->flyout = Flyout_create(0, b);
	b->flowgrid = FlowGrid_create(b);
	FlowGrid_setSpacing(b->flowgrid, (Size){0, 0});
	Widget_setPadding(b->flowgrid, (Box){0, 0, 0, 0});
	Widget_setFont(b->flowgrid, Widget_font(b));
	Widget_setBackground(b->flowgrid, 1, COLOR_BG_NORMAL);
	Flyout_setWidget(b->flyout, b->flowgrid);
    }
    FlowGrid_addWidget(b->flowgrid, variant);
}

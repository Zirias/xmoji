#include "emojibutton.h"

#include "flowgrid.h"
#include "flyout.h"
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
    xcb_pixmap_t pixmap;
    xcb_render_picture_t pic;
    xcb_render_picture_t pen;
    Color pencolor;
    int selected;
};

static void destroy(void *obj)
{
    EmojiButton *self = obj;
    if (self->pen)
    {
	xcb_connection_t *c = X11Adapter_connection();
	xcb_render_free_picture(c, self->pen);
	xcb_render_free_picture(c, self->pic);
	xcb_free_pixmap(c, self->pixmap);
    }
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

static void prerender(EmojiButton *self, Size newSize)
{
    if (!self->flyout) return;

    xcb_connection_t *c = X11Adapter_connection();
    xcb_render_picture_t picture = Widget_picture(self);

    if (!self->pen)
    {
	xcb_pixmap_t p = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 24, p, X11Adapter_screen()->root, 1, 1),
		"Cannot create pen pixmap for 0x%x", (unsigned)picture);
	self->pen = xcb_generate_id(c);
	uint32_t repeat = XCB_RENDER_REPEAT_NORMAL;
	CHECK(xcb_render_create_picture(c, self->pen, p,
		    X11Adapter_rgbformat(), XCB_RENDER_CP_REPEAT, &repeat),
		"Cannot create pen for 0x%x", (unsigned)picture);
	Color fill = 0xffffffff;
	xcb_rectangle_t rect = {0, 0, 1, 1};
	CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
		    self->pen, Color_xcb(fill), 1, &rect),
		"Canot colorize pen for 0x%x", (unsigned)picture);
	xcb_free_pixmap(c, p);
    }
    if (self->pic)
    {
	xcb_render_free_picture(c, self->pic);
	xcb_free_pixmap(c, self->pixmap);
    }
    self->pixmap = xcb_generate_id(c);
    CHECK(xcb_create_pixmap(c, 8, self->pixmap,
		X11Adapter_screen()->root,
		newSize.width, newSize.height),
	    "Cannot create triangle pixmap for 0x%x",
	    (unsigned)picture);
    self->pic = xcb_generate_id(c);
    CHECK(xcb_render_create_picture(c, self->pic, self->pixmap,
		X11Adapter_alphaformat(), 0, 0),
	    "Cannot create triangle picture for 0x%x",
	    (unsigned)picture);
    Color clear = 0;
    xcb_rectangle_t rect = { 0, 0, newSize.width, newSize.height };
    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC,
		self->pic, Color_xcb(clear), 1, &rect),
	    "Cannot clear triangle picture for 0x%x",
	    (unsigned)picture);
    uint32_t x = newSize.width << 16;
    uint32_t y = newSize.height << 16;
    xcb_render_triangle_t triangle = {
	{ x - (newSize.width << 14), y },
	{ x, y },
	{ x, y - (newSize.height << 14) }
    };
    CHECK(xcb_render_triangles(c, XCB_RENDER_PICT_OP_OVER,
		self->pen, self->pic, 0, 0, 0, 1, &triangle),
	"Cannot render fly-out indicator for 0x%x", (unsigned)picture);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    EmojiButton *self = Object_instance(obj);
    int rc = 0;
    Object_bcall(rc, Widget, draw, self, picture);
    if (rc == 0 && picture && self->flyout)
    {
	xcb_connection_t *c = X11Adapter_connection();
	Rect geom = Widget_geometry(self);
	if (!self->pencolor) prerender(self, geom.size);
	Color pencolor = Widget_color(self, COLOR_ACTIVE);
	if (pencolor != self->pencolor)
	{
	    self->pencolor = pencolor;
	    xcb_rectangle_t rect = {0, 0, 1, 1};
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			self->pen, Color_xcb(pencolor), 1, &rect),
		    "Canot colorize pen for 0x%x", (unsigned)picture);
	}
	CHECK(xcb_render_composite(c, XCB_RENDER_PICT_OP_OVER, self->pen,
		    self->pic, picture, 0, 0, 0, 0,
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

    if (ea->newSize.width && ea->newSize.height) prerender(self, ea->newSize);
}

EmojiButton *EmojiButton_createBase(void *derived,
	const char *name, void *parent)
{
    EmojiButton *self = PSC_malloc(sizeof *self);
    CREATEBASE(Button, name, parent);
    self->flowgrid = 0;
    self->flyout = 0;
    self->pixmap = 0;
    self->pic = 0;
    self->pen = 0;
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

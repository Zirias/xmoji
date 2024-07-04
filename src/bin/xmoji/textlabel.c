#include "textlabel.h"

#include "font.h"
#include "textrenderer.h"
#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static void setFont(void *obj, Font *font);
static Size minSize(const void *obj);

static MetaTextLabel mo = MetaTextLabel_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, setFont, 0, minSize, 0, 0, 0,
	"TextLabel", destroy);

#define dummy ((void *)&mo)

struct TextLabel
{
    Object base;
    UniStr *text;
    PSC_List *renderers;
    RenderCallback callback;
    void *renderctx;
    Size minSize;
    ColorRole color;
};

static void freerenderer(void *renderer)
{
    TextRenderer_destroy(renderer);
}

static void update(TextLabel *self, Font *font)
{
    PSC_List_destroy(self->renderers);
    self->renderers = 0;
    self->minSize = (Size){0, 0};
    if (self->text)
    {
	self->renderers = PSC_List_create();
	PSC_List *lines = UniStr_split(self->text, U"\n");
	PSC_ListIterator *i = PSC_List_iterator(lines);
	while (PSC_ListIterator_moveNext(i))
	{
	    const UniStr *line = PSC_ListIterator_current(i);
	    if (UniStr_len(line))
	    {
		TextRenderer *renderer = TextRenderer_create(self->base.base);
		TextRenderer_setFont(renderer, font);
		PSC_List_append(self->renderers, renderer, freerenderer);
		TextRenderer_setText(renderer, line);
		if (self->callback) self->callback(self->renderctx, renderer);
		Size linesize = TextRenderer_size(renderer);
		if (!self->minSize.height)
		{
		    self->minSize.height = linesize.height;
		}
		if (linesize.width > self->minSize.width)
		{
		    self->minSize.width = linesize.width;
		}
	    }
	    else PSC_List_append(self->renderers, dummy, 0);
	}
	PSC_ListIterator_destroy(i);
	PSC_List_destroy(lines);
	size_t nlines = PSC_List_size(self->renderers);
	if (nlines > 1)
	{
	    self->minSize.height += (nlines - 1) * Font_linespace(font);
	}
    }
    Widget_requestSize(self);
}

static void destroy(void *obj)
{
    TextLabel *self = obj;
    PSC_List_destroy(self->renderers);
    UniStr_destroy(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    TextLabel *self = Object_instance(obj);
    if (!picture || !self->text) return 0;
    Font *font = Widget_font(self);
    if (!font) return 0;
    Color color = Widget_color(self, self->color);
    Pos pos = Widget_contentOrigin(self, self->minSize);
    Align align = Widget_align(self);
    uint16_t linespace = Font_linespace(font);
    int rc = 0;
    for (size_t i = 0; rc >= 0 && i < PSC_List_size(self->renderers); ++i)
    {
	TextRenderer *r = PSC_List_at(self->renderers, i);
	if (r != dummy)
	{
	    Size lineSize = TextRenderer_size(r);
	    Pos linePos = pos;
	    int lineMargin = self->minSize.width - lineSize.width;
	    if (lineMargin > 0)
	    {
		if (align & AH_RIGHT) linePos.x += lineMargin;
		else if (align & AH_CENTER) linePos.x += lineMargin / 2;
	    }
	    rc = TextRenderer_render(r, picture, color, linePos);
	}
	pos.y += linespace;
    }
    return rc;
}

static void setFont(void *obj, Font *font)
{
    update(Object_instance(obj), font);
}

static Size minSize(const void *obj)
{
    const TextLabel *self = Object_instance(obj);
    return self->minSize;
}

TextLabel *TextLabel_createBase(void *derived, const char *name, void *parent)
{
    TextLabel *self = PSC_malloc(sizeof *self);
    CREATEBASE(Widget, name, parent);
    self->text = 0;
    self->renderers = PSC_List_create();
    self->callback = 0;
    self->renderctx = 0;
    self->minSize = (Size){0, 0};
    self->color = COLOR_NORMAL;

    return self;
}

const UniStr *TextLabel_text(const void *self)
{
    TextLabel *l = Object_instance(self);
    return l->text;
}

void TextLabel_setText(void *self, const UniStr *text)
{
    TextLabel *l = Object_instance(self);
    UniStr_destroy(l->text);
    l->text = UniStr_ref(text);
    Font *font = Widget_font(l);
    if (font) update(l, font);
}

void TextLabel_setColor(void *self, ColorRole color)
{
    TextLabel *l = Object_instance(self);
    if (l->color != color)
    {
	l->color = color;
	Widget_invalidate(l);
    }
}

void TextLabel_setRenderCallback(void *self, void *ctx,
	RenderCallback callback)
{
    TextLabel *l = Object_instance(self);
    l->callback = callback;
    l->renderctx = ctx;
}


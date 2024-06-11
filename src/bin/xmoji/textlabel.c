#include "textlabel.h"

#include "font.h"
#include "textrenderer.h"
#include "unistr.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);

static MetaTextLabel mo = MetaTextLabel_init(
	0, draw, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, minSize, 0, 0, 0,
	"TextLabel", destroy);

#define dummy ((void *)&mo)

struct TextLabel
{
    Object base;
    Font *font;
    UniStr *text;
    PSC_List *renderers;
    Size minSize;
};

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
    Color color = Widget_color(self, COLOR_NORMAL);
    Pos pos = Widget_contentOrigin(self, self->minSize);
    Align align = Widget_align(self);
    uint16_t linespace = Font_linespace(self->font);
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

static Size minSize(const void *obj)
{
    const TextLabel *self = Object_instance(obj);
    return self->minSize;
}

static void freerenderer(void *renderer)
{
    TextRenderer_destroy(renderer);
}

TextLabel *TextLabel_createBase(void *derived, const char *name,
	void *parent, Font *font)
{
    REGTYPE(0);

    TextLabel *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.type = OBJTYPE;
    self->base.base = Widget_createBase(derived, name, parent);
    self->font = font;
    self->text = 0;
    self->renderers = PSC_List_create();
    self->minSize = (Size){0, 0};

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
    PSC_List_destroy(l->renderers);
    UniStr_destroy(l->text);
    l->renderers = 0;
    l->text = 0;
    l->minSize = (Size){0, 0};
    if (text)
    {
	l->text = UniStr_ref(text);
	l->renderers = PSC_List_create();
	PSC_List *lines = UniStr_split(text, "\n");
	PSC_ListIterator *i = PSC_List_iterator(lines);
	while (PSC_ListIterator_moveNext(i))
	{
	    const UniStr *line = PSC_ListIterator_current(i);
	    if (UniStr_len(line))
	    {
		TextRenderer *renderer = TextRenderer_create(l->font);
		PSC_List_append(l->renderers, renderer, freerenderer);
		TextRenderer_setText(renderer, line);
		Size linesize = TextRenderer_size(renderer);
		if (!l->minSize.height)
		{
		    l->minSize.height = linesize.height;
		}
		if (linesize.width > l->minSize.width)
		{
		    l->minSize.width = linesize.width;
		}
	    }
	    else PSC_List_append(l->renderers, dummy, 0);
	}
	PSC_ListIterator_destroy(i);
	PSC_List_destroy(lines);
	size_t nlines = PSC_List_size(l->renderers);
	if (nlines > 1)
	{
	    l->minSize.height += (nlines - 1) * Font_linespace(l->font);
	}
    }
    Widget_requestSize(self);
}


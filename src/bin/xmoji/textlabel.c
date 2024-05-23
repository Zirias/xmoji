#include "textlabel.h"

#include "font.h"
#include "textrenderer.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);

static MetaTextLabel mo = MetaTextLabel_init("TextLabel",
	destroy, draw, 0, 0, minSize);

#define dummy ((void *)&mo)

struct TextLabel
{
    Object base;
    Font *font;
    char *text;
    PSC_List *renderers;
    Size minSize;
};

static void destroy(void *obj)
{
    TextLabel *self = obj;
    PSC_List_destroy(self->renderers);
    free(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    TextLabel *self = Object_instance(obj);
    if (!self->text) return 0;
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

TextLabel *TextLabel_createBase(void *derived, void *parent, Font *font)
{
    REGTYPE(0);

    TextLabel *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.base = Widget_createBase(derived, parent);
    self->base.type = OBJTYPE;
    self->font = font;
    self->text = 0;
    self->renderers = PSC_List_create();
    self->minSize = (Size){0, 0};

    return self;
}

const char *TextLabel_text(const void *self)
{
    TextLabel *l = Object_instance(self);
    return l->text;
}

void TextLabel_setText(void *self, const char *text)
{
    TextLabel *l = Object_instance(self);
    PSC_List_destroy(l->renderers);
    free(l->text);
    l->text = PSC_copystr(text);
    l->minSize = (Size){0, 0};
    if (text)
    {
	l->renderers = PSC_List_create();
	int len = 0;
	for (const char *line = text; *line; line += len)
	{
	    for (len = 0; line[len] && line[len] != '\n'; ++len);
	    if (len)
	    {
		TextRenderer *renderer = TextRenderer_create(l->font);
		PSC_List_append(l->renderers, renderer, freerenderer);
		TextRenderer_setUtf8(renderer, line, len);
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
	    if (line[len]) ++len;
	}
	size_t lines = PSC_List_size(l->renderers);
	if (lines > 1)
	{
	    l->minSize.height += (lines - 1) * Font_linespace(l->font);
	}
    }
    Widget_requestSize(self);
}


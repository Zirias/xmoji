#include "textlabel.h"

#include "textrenderer.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);
static int draw(void *obj, xcb_render_picture_t picture);
static Size minSize(const void *obj);

static MetaTextLabel mo = MetaTextLabel_init("TextLabel",
	destroy, draw, 0, 0, minSize);

struct TextLabel
{
    Object base;
    char *text;
    TextRenderer *renderer;
    Size minSize;
};

static void destroy(void *obj)
{
    TextLabel *self = obj;
    TextRenderer_destroy(self->renderer);
    free(self->text);
    free(self);
}

static int draw(void *obj, xcb_render_picture_t picture)
{
    TextLabel *self = Object_instance(obj);
    if (!self->text) return 0;
    return TextRenderer_render(self->renderer, picture,
	    Widget_color(self, COLOR_NORMAL),
	    Widget_contentOrigin(self, self->minSize));
}

static Size minSize(const void *obj)
{
    const TextLabel *self = Object_instance(obj);
    return self->minSize;
}

static void textshaped(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    TextLabel *self = receiver;
    self->minSize = TextRenderer_size(self->renderer);
    Widget_requestSize(self);
}

TextLabel *TextLabel_createBase(void *derived, void *parent, Font *font)
{
    REGTYPE(0);

    TextLabel *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.base = Widget_createBase(derived, parent);
    self->base.type = OBJTYPE;
    self->text = 0;
    self->renderer = TextRenderer_create(font);
    self->minSize = (Size){0, 0};

    PSC_Event_register(TextRenderer_shaped(self->renderer),
	    self, textshaped, 0);

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
    free(l->text);
    l->text = PSC_copystr(text);
    if (text) TextRenderer_setUtf8(l->renderer, text);
    else
    {
	l->minSize = (Size){0, 0};
	Widget_requestSize(self);
    }
}

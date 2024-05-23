#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(TextRenderer);

TextRenderer *TextRenderer_create(Font *font)
    ATTR_NONNULL((1));
Size TextRenderer_size(const TextRenderer *self)
    CMETHOD;
int TextRenderer_setUtf8(TextRenderer *self, const char *utf8, int len)
    CMETHOD;
int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos)
    CMETHOD;
void TextRenderer_destroy(TextRenderer *self);

#endif

#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(TextRenderer);
C_CLASS_DECL(UniStr);

TextRenderer *TextRenderer_create(Font *font)
    ATTR_NONNULL((1));
Size TextRenderer_size(const TextRenderer *self)
    CMETHOD;
int TextRenderer_setText(TextRenderer *self, const UniStr *text)
    CMETHOD;
unsigned TextRenderer_pixelOffset(TextRenderer *self, unsigned index)
    CMETHOD;
int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos)
    CMETHOD;
void TextRenderer_destroy(TextRenderer *self);

#endif

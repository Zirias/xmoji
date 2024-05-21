#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(TextRenderer);

TextRenderer *TextRenderer_create(Font *font)
    ATTR_NONNULL((1));
PSC_Event *TextRenderer_shaped(TextRenderer *self)
    CMETHOD;
Size TextRenderer_size(const TextRenderer *self)
    CMETHOD;
int TextRenderer_setUtf8(TextRenderer *self, const char *utf8)
    CMETHOD;
void TextRenderer_setColor(TextRenderer *self, Color color)
    CMETHOD;
int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Pos pos)
    CMETHOD;
void TextRenderer_destroy(TextRenderer *self);

xcb_render_picture_t TextRenderer_createPicture(xcb_drawable_t drawable);

#endif

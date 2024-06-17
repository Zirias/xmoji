#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include "valuetypes.h"

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(TextRenderer);
C_CLASS_DECL(UniStr);
C_CLASS_DECL(Widget);

TextRenderer *TextRenderer_create(Widget *owner);
Size TextRenderer_size(const TextRenderer *self)
    CMETHOD;
void TextRenderer_setNoLigatures(TextRenderer *self, int noLigatures)
    CMETHOD;
void TextRenderer_setFont(TextRenderer *self, Font *font);
int TextRenderer_setText(TextRenderer *self, const UniStr *text)
    CMETHOD;
unsigned TextRenderer_glyphLen(const TextRenderer *self, unsigned index)
    CMETHOD;
unsigned TextRenderer_pixelOffset(const TextRenderer *self, unsigned index)
    CMETHOD;
unsigned TextRenderer_charIndex(const TextRenderer *self, unsigned pixelpos)
    CMETHOD;
int TextRenderer_renderWithSelection(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos,
	Selection selection, Color selectionColor)
    CMETHOD;
int TextRenderer_render(TextRenderer *self,
	xcb_render_picture_t picture, Color color, Pos pos)
    CMETHOD;
void TextRenderer_destroy(TextRenderer *self);

#endif

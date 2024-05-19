#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/render.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(TextRenderer);

typedef void (*TR_size_cb)(void *ctx, uint32_t width, uint32_t height);

TextRenderer *TextRenderer_fromUtf8(Font *font, const char *utf8);
int TextRenderer_size(TextRenderer *self, void *ctx, TR_size_cb cb)
    CMETHOD;
xcb_render_picture_t TextRenderer_createPicture(
	TextRenderer *self, xcb_drawable_t drawable)
    CMETHOD;
void TextRenderer_render(TextRenderer *self, xcb_render_picture_t picture)
    CMETHOD;
void TextRenderer_destroy(TextRenderer *self);

#endif

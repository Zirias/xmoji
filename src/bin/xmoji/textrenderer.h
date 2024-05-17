#ifndef XMOJI_TEXTRENDERER_H
#define XMOJI_TEXTRENDERER_H

#include <poser/decl.h>
#include <stdint.h>
#include <xcb/xcb.h>

C_CLASS_DECL(Font);
C_CLASS_DECL(TextRenderer);

typedef void (*TR_size_cb)(void *ctx, uint32_t width, uint32_t height);
typedef void (*TR_render_cb)(void *ctx, int rc);

TextRenderer *TextRenderer_fromUtf8(Font *font, const char *utf8);
int TextRenderer_size(TextRenderer *self, void *ctx, TR_size_cb cb);
void TextRenderer_render(TextRenderer *self, xcb_drawable_t drawable,
	void *ctx, TR_render_cb cb);
void TextRenderer_destroy(TextRenderer *self);

#endif

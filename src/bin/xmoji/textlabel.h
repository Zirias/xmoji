#ifndef XMOJI_TEXTLABEL_H
#define XMOJI_TEXTLABEL_H

#include "widget.h"

typedef struct MetaTextLabel
{
    MetaWidget base;
} MetaTextLabel;

#define MetaTextLabel_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(TextLabel);
C_CLASS_DECL(TextRenderer);
C_CLASS_DECL(UniStr);

typedef void (*RenderCallback)(void *ctx, TextRenderer *renderer);

TextLabel *TextLabel_createBase(void *derived, const char *name, void *parent);
#define TextLabel_create(...) TextLabel_createBase(0, __VA_ARGS__)
const UniStr *TextLabel_text(const void *self) CMETHOD;
void TextLabel_setText(void *self, const UniStr *text) CMETHOD;
void TextLabel_setColor(void *self, ColorRole color) CMETHOD;
void TextLabel_setRenderCallback(void *self, void *ctx,
	RenderCallback callback) CMETHOD;

#endif

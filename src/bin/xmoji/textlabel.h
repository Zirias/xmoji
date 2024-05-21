#ifndef XMOJI_TEXTLABEL_H
#define XMOJI_TEXTLABEL_H

#include "widget.h"

typedef struct MetaTextLabel
{
    MetaWidget base;
} MetaTextLabel;

#define MetaTextLabel_init(name, destroy, draw, show, hide, minSize) { \
    .base = MetaWidget_init(name, destroy, draw, show, hide, minSize) \
}

C_CLASS_DECL(Font);
C_CLASS_DECL(TextLabel);

TextLabel *TextLabel_createBase(void *derived, void *parent, Font *font);
#define TextLabel_create(...) TextLabel_createBase(0, __VA_ARGS__)
const char *TextLabel_text(const void *self) CMETHOD;
void TextLabel_setText(void *self, const char *text) CMETHOD;

#endif

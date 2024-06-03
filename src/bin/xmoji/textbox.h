#ifndef XMOJI_TEXTBOX_H
#define XMOJI_TEXTBOX_H

#include "widget.h"

typedef struct MetaTextBox
{
    MetaWidget base;
} MetaTextBox;

#define MetaTextBox_init(name, destroy, \
	draw, show, hide, minSize, keyPressed) { \
    .base = MetaWidget_init(name, destroy, \
	    draw, show, hide, minSize, keyPressed) \
}

C_CLASS_DECL(Font);
C_CLASS_DECL(TextBox);
C_CLASS_DECL(UniStr);

TextBox *TextBox_createBase(void *derived, void *parent, Font *font);
#define TextBox_create(...) TextBox_createBase(0, __VA_ARGS__)
const UniStr *TextBox_text(const void *self) CMETHOD;
void TextBox_setText(void *self, const UniStr *text) CMETHOD;

#endif
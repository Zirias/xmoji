#ifndef XMOJI_TEXTBOX_H
#define XMOJI_TEXTBOX_H

#include "widget.h"

typedef struct MetaTextBox
{
    MetaWidget base;
} MetaTextBox;

#define MetaTextBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(TextBox);
C_CLASS_DECL(UniStr);

TextBox *TextBox_createBase(void *derived, const char *name, void *parent);
#define TextBox_create(...) TextBox_createBase(0, __VA_ARGS__)
const UniStr *TextBox_text(const void *self) CMETHOD;
PSC_Event *TextBox_textChanged(void *self) CMETHOD ATTR_RETNONNULL;
void TextBox_setText(void *self, const UniStr *text) CMETHOD;
void TextBox_setPlaceholder(void *self, const UniStr *text) CMETHOD;
void TextBox_setGrab(void *self, int grab) CMETHOD;

#endif

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

typedef int (*InputFilter)(void *obj, const UniStr *str);

TextBox *TextBox_createBase(void *derived, const char *name, void *parent);
#define TextBox_create(...) TextBox_createBase(0, __VA_ARGS__)
const UniStr *TextBox_text(const void *self) CMETHOD;
PSC_Event *TextBox_textChanged(void *self) CMETHOD ATTR_RETNONNULL;
void TextBox_setText(void *self, const UniStr *text) CMETHOD;
unsigned TextBox_maxLen(const void *self) CMETHOD;
void TextBox_setMaxLen(void *self, unsigned len) CMETHOD;
void TextBox_setPlaceholder(void *self, const UniStr *text) CMETHOD;
void TextBox_setGrab(void *self, int grab) CMETHOD;
void TextBox_setClearBtn(void *self, int enabled) CMETHOD;
void TextBox_setInputFilter(void *self, void *obj, InputFilter filter) CMETHOD;

#endif

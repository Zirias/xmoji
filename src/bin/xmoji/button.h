#ifndef XMOJI_BUTTON_H
#define XMOJI_BUTTON_H

#include "widget.h"

typedef struct MetaButton
{
    MetaWidget base;
} MetaButton;

#define MetaButton_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(Button);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(UniStr);

Button *Button_createBase(void *derived, const char *name, void *parent);
#define Button_create(...) Button_createBase(0, __VA_ARGS__)
PSC_Event *Button_clicked(void *self) CMETHOD;
const UniStr *Button_text(const void *self) CMETHOD;
void Button_setText(void *self, const UniStr *text) CMETHOD;

#endif

#ifndef XMOJI_SCROLLBOX_H
#define XMOJI_SCROLLBOX_H

#include "widget.h"

typedef struct MetaScrollBox
{
    MetaWidget base;
} MetaScrollBox;

#define MetaScrollBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(ScrollBox);

ScrollBox *ScrollBox_createBase(void *derived, const char *name, void *parent);
#define ScrollBox_create(...) ScrollBox_createBase(0, __VA_ARGS__)
void ScrollBox_setWidget(void *self, void *widget) CMETHOD;

#endif

#ifndef XMOJI_VBOX_H
#define XMOJI_VBOX_H

#include "widget.h"

typedef struct MetaVBox
{
    MetaWidget base;
} MetaVBox;

#define MetaVBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(VBox);

VBox *VBox_createBase(void *derived, void *parent);
#define VBox_create(...) VBox_createBase(0, __VA_ARGS__)
void VBox_addWidget(void *self, void *widget) CMETHOD;

#endif

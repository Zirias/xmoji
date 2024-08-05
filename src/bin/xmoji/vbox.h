#ifndef XMOJI_VBOX_H
#define XMOJI_VBOX_H

#include "widget.h"

typedef struct MetaVBox
{
    MetaWidget base;
    void (*layout)(void *vbox, int updateMinSize);
} MetaVBox;

#define MetaVBox_init(mlayout, ...) { \
    .layout = mlayout, \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(VBox);

VBox *VBox_createBase(void *derived, void *parent);
#define VBox_create(...) VBox_createBase(0, __VA_ARGS__)
void VBox_addWidget(void *self, void *widget) CMETHOD;
uint16_t VBox_spacing(const void *self) CMETHOD;
void VBox_setSpacing(void *self, uint16_t spacing) CMETHOD;

/* "protected" */
unsigned VBox_rows(const void *self) CMETHOD;
void *VBox_widget(void *self, unsigned row) CMETHOD;

#endif

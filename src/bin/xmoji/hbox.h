#ifndef XMOJI_HBOX_H
#define XMOJI_HBOX_H

#include "widget.h"

typedef struct MetaHBox
{
    MetaWidget base;
} MetaHBox;

#define MetaHBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(HBox);

HBox *HBox_createBase(void *derived, void *parent);
#define HBox_create(...) HBox_createBase(0, __VA_ARGS__)
void HBox_addWidget(void *self, void *widget) CMETHOD;
uint16_t HBox_spacing(const void *self) CMETHOD;
void HBox_setSpacing(void *self, uint16_t spacing) CMETHOD;

/* "protected" */
unsigned HBox_cols(const void *self) CMETHOD;
uint16_t HBox_minWidth(const void *self, unsigned col) CMETHOD;
void HBox_setMinWidth(void *self, unsigned col, uint16_t minWidth) CMETHOD;

#endif

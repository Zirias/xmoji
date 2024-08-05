#ifndef XMOJI_SPINBOX_H
#define XMOJI_SPINBOX_H

#include "widget.h"

typedef struct MetaSpinBox
{
    MetaWidget base;
} MetaSpinBox;

#define MetaSpinBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(SpinBox);

SpinBox *SpinBox_createBase(void *derived, const char *name,
	int min, int max, int step, void *parent);
#define SpinBox_create(...) SpinBox_createBase(0, __VA_ARGS__)
int SpinBox_value(const void *self) CMETHOD;
PSC_Event *SpinBox_valueChanged(void *self) CMETHOD ATTR_RETNONNULL;
void SpinBox_setValue(void *self, int value) CMETHOD;

#endif

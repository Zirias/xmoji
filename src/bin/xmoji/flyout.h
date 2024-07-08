#ifndef XMOJI_FLYOUT_H
#define XMOJI_FLYOUT_H

#include "widget.h"

typedef struct MetaFlyout
{
    MetaWidget base;
} MetaFlyout;

#define MetaFlyout_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(Flyout);

Flyout *Flyout_createBase(void *derived, const char *name, void *parent);
#define Flyout_create(...) Flyout_createBase(0, __VA_ARGS__)
void Flyout_setWidget(void *self, void *widget) CMETHOD;
void Flyout_popup(void *self, void *widget) CMETHOD;

#endif

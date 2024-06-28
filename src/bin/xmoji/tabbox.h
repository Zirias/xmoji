#ifndef XMOJI_TABBOX_H
#define XMOJI_TABBOX_H

#include "widget.h"

typedef struct MetaTabBox
{
    MetaWidget base;
} MetaTabBox;

#define MetaTabBox_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(TabBox);

TabBox *TabBox_createBase(void *derived, const char *name, void *parent);
#define TabBox_create(...) TabBox_createBase(0, __VA_ARGS__)
void TabBox_addTab(void *self, void *buttonWidget, void *contentWidget)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3));

#endif

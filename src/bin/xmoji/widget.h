#ifndef XMOJI_WIDGET_H
#define XMOJI_WIDGET_H

#include "object.h"
#include "valuetypes.h"

#include <poser/decl.h>

typedef struct MetaWidget
{
    MetaObject base;
    int (*draw)(void *widget);
    int (*show)(void *widget);
    int (*hide)(void *widget);
    Size (*minSize)(const void *widget);
} MetaWidget;

#define MetaWidget_init(name, destroy, mdraw, mshow, mhide, mminSize) { \
    .base = MetaObject_init(name, destroy), \
    .draw = mdraw, \
    .show = mshow, \
    .hide = mhide, \
    .minSize = mminSize \
}

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(Widget);

typedef struct SizeChangedEventArgs
{
    Size oldSize;
    Size newSize;
} SizeChangedEventArgs;

Widget *Widget_create(void *parent);
PSC_Event *Widget_shown(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_hidden(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeRequested(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeChanged(void *self) CMETHOD ATTR_RETNONNULL;
int Widget_draw(void *self) CMETHOD;
int Widget_show(void *self) CMETHOD;
int Widget_hide(void *self) CMETHOD;
void Widget_setSize(void *self, Size size) CMETHOD;
Size Widget_minSize(const void *self) CMETHOD;
Size Widget_size(const void *self) CMETHOD;
int Widget_visible(const void *self) CMETHOD;

#endif

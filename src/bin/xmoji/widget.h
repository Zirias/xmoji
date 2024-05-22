#ifndef XMOJI_WIDGET_H
#define XMOJI_WIDGET_H

#include "colorset.h"
#include "object.h"
#include "valuetypes.h"

#include <poser/decl.h>
#include <xcb/render.h>

typedef struct MetaWidget
{
    MetaObject base;
    int (*draw)(void *widget, xcb_render_picture_t picture);
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

Widget *Widget_createBase(void *derived, void *parent);
#define Widget_create(...) Widget_createBase(0, __VA_ARGS__)
PSC_Event *Widget_shown(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_hidden(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeRequested(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeChanged(void *self) CMETHOD ATTR_RETNONNULL;
Widget *Widget_parent(const void *self) CMETHOD;
int Widget_draw(void *self) CMETHOD;
int Widget_show(void *self) CMETHOD;
int Widget_hide(void *self) CMETHOD;
void Widget_setSize(void *self, Size size) CMETHOD;
Size Widget_minSize(const void *self) CMETHOD;
Size Widget_size(const void *self) CMETHOD;
void Widget_setOrigin(void *self, Pos origin) CMETHOD;
Pos Widget_origin(const void *self) CMETHOD;
const ColorSet *Widget_colorSet(const void *self) CMETHOD;
Color Widget_color(const void *self, ColorRole role) CMETHOD;
void Widget_setColor(void *self, ColorRole role, Color color) CMETHOD;
void Widget_setBackground(void *self, int enabled, ColorRole role) CMETHOD;
xcb_drawable_t Widget_drawable(const void *self) CMETHOD;
void Widget_setDrawable(void *self, xcb_drawable_t drawable) CMETHOD;
int Widget_visible(const void *self) CMETHOD;

void Widget_requestSize(void *self) CMETHOD;

#endif

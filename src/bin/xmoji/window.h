#ifndef XMOJI_WINDOW_H
#define XMOJI_WINDOW_H

#include "valuetypes.h"
#include "widget.h"

#include <poser/decl.h>
#include <xcb/xproto.h>

typedef struct MetaWindow
{
    MetaWidget base;
} MetaWindow;

#define MetaWindow_init(name, destroy, draw, show, hide, minSize) { \
    .base = MetaWidget_init(name, destroy, draw, show, hide, minSize) \
}

C_CLASS_DECL(Window);

Window *Window_createBase(void *derived, void *parent);
#define Window_create(...) Window_createBase(0, __VA_ARGS__)

xcb_window_t Window_id(void *self)
    CMETHOD;

PSC_Event *Window_closed(void *self)
    CMETHOD ATTR_RETNONNULL;

PSC_Event *Window_errored(void *self)
    CMETHOD ATTR_RETNONNULL;

void Window_setBackgroundColor(void *self, Color color)
    CMETHOD;
void Window_setDefaultColor(void *self, Color color)
    CMETHOD;

const char *Window_title(const void *self)
    CMETHOD;
void Window_setTitle(void *self, const char *title)
    CMETHOD;

#endif

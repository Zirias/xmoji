#ifndef XMOJI_WINDOW_H
#define XMOJI_WINDOW_H

#include "object.h"
#include <poser/decl.h>
#include <xcb/xproto.h>

typedef struct MetaWindow
{
    MetaObject base;
    int (*show)(void *window);
    int (*hide)(void *window);
} MetaWindow;

#define MetaWindow_init(name, destroy, mshow, mhide) { \
    .base = MetaObject_init(name, destroy), \
    .show = mshow, \
    .hide = mhide \
}

C_CLASS_DECL(Window);
C_CLASS_DECL(PSC_Event);

Window *Window_create(void);

xcb_window_t Window_id(void *self)
    CMETHOD;

PSC_Event *Window_closed(void *self)
    CMETHOD ATTR_RETNONNULL;

void Window_show(void *self)
    CMETHOD;

void Window_hide(void *self)
    CMETHOD;

uint32_t Window_width(const void *self)
    CMETHOD;
uint32_t Window_height(const void *self)
    CMETHOD;
void Window_setSize(void *self, uint32_t width, uint32_t height)
    CMETHOD;

const char *Window_title(const void *self)
    CMETHOD;
void Window_setTitle(void *self, const char *title)
    CMETHOD;

#define Window_destroy(w) Object_destroy(w)

#endif

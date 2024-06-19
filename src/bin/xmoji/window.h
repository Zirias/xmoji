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

#define MetaWindow_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(UniStr);
C_CLASS_DECL(Window);
C_CLASS_DECL(XSelection);

typedef enum WindowState
{
    WS_NONE,
    WS_MINIMIZED,
    WS_NORMAL
} WindowState;

Window *Window_createBase(void *derived, const char *name, void *parent);
#define Window_create(...) Window_createBase(0, __VA_ARGS__)

Window *Window_fromWidget(void *widget)
    ATTR_NONNULL((1));

xcb_window_t Window_id(const void *self)
    CMETHOD ATTR_PURE;

PSC_Event *Window_closed(void *self)
    CMETHOD ATTR_RETNONNULL;

PSC_Event *Window_propertyChanged(void *self)
    CMETHOD ATTR_RETNONNULL;

const char *Window_title(const void *self)
    CMETHOD;
void Window_setTitle(void *self, const char *title)
    CMETHOD;

const char *Window_iconName(const void *self)
    CMETHOD;
void Window_setIconName(void *self, const char *iconName)
    CMETHOD;

void *Window_mainWidget(const void *self)
    CMETHOD;
void Window_setMainWidget(void *self, void *widget)
    CMETHOD;

void Window_setFocusWidget(void *self, void *widget)
    CMETHOD;

xcb_atom_t Window_takeProperty(void *self)
    CMETHOD;
void Window_returnProperty(void *self, xcb_atom_t property)
    CMETHOD;

XSelection *Window_primary(void *self)
    CMETHOD;
XSelection *Window_clipboard(void *self)
    CMETHOD;

WindowState Window_state(const void *self)
    CMETHOD;
void Window_close(void *self)
    CMETHOD;

void Window_showTooltip(void *self, void *widget, void *parentWidget)
    CMETHOD ATTR_NONNULL((2));

void Window_invalidateHover(void *self)
    CMETHOD;

#endif

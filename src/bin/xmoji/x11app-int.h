#ifndef XMOJI_X11APP_INT_H
#define XMOJI_X11APP_INT_H

#include "x11adapter.h"
#include "x11app.h"

void X11App_raiseError(X11App *self, Window *window, Widget *widget,
	RequestErrorEventArgs *err)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3)) ATTR_NONNULL((4));

#endif

#ifndef XMOJI_X11APP_H
#define XMOJI_X11APP_H

#include "object.h"

#include <poser/decl.h>

C_CLASS_DECL(X11App);

typedef struct MetaX11App
{
    MetaObject base;
    int (*startup)(void *app);
    void (*shutdown)(void *app);
} MetaX11App;

#define MetaX11App_init(mstartup, mshutdown, ...) { \
    .base = MetaObject_init(__VA_ARGS__), \
    .startup = mstartup, \
    .shutdown = mshutdown, \
}

X11App *app(void);

X11App *X11App_createBase(void *derived, int argc, char **argv);
#define X11App_create(...) X11App_createBase(0, __VA_ARGS__)
int X11App_run(void);
void X11App_quit(void);

#endif

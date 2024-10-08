#ifndef XMOJI_X11APP_H
#define XMOJI_X11APP_H

#include "object.h"

#include <poser/decl.h>

C_CLASS_DECL(X11Error);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(Widget);
C_CLASS_DECL(Window);
C_CLASS_DECL(X11App);

typedef struct MetaX11App
{
    MetaObject base;
    int (*prestartup)(void *app);
    int (*startup)(void *app);
    void (*shutdown)(void *app);
} MetaX11App;

#define MetaX11App_init(mprestartup, mstartup, mshutdown, ...) { \
    .base = MetaObject_init(__VA_ARGS__), \
    .prestartup = mprestartup, \
    .startup = mstartup, \
    .shutdown = mshutdown, \
}

X11App *app(void);

X11App *X11App_createBase(void *derived, int argc, char **argv);
#define X11App_create(...) X11App_createBase(0, __VA_ARGS__)
int X11App_run(void);
void X11App_quit(void);
PSC_Event *X11App_error(void);

Window *X11Error_window(X11Error *self) CMETHOD;
Widget *X11Error_widget(X11Error *self) CMETHOD;
uint8_t X11Error_code(X11Error *self) CMETHOD;
uint8_t X11Error_opMajor(X11Error *self) CMETHOD;
uint16_t X11Error_opMinor(X11Error *self) CMETHOD;
void X11Error_ignore(X11Error *self) CMETHOD;

void X11App_showWaitCursor(void);

const char *X11App_name(void);
const char *X11App_lcMessages(void);
const char *X11App_hostname(void);

#endif

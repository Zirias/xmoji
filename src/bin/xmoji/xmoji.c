#include "xmoji.h"

#include "font.h"
#include "window.h"
#include "x11adapter.h"

#include <inttypes.h>
#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static Font *sysfont;
static Font *emojifont;
static Window *win;

static struct { int argc; char **argv; } startupargs;

static void onclose(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Service_quit();
}

static void onprestartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;

    PSC_EAStartup *ea = args;

    if (X11Adapter_init(
		startupargs.argc, startupargs.argv, "Xmoji") < 0) goto error;
    if (Font_init() < 0) goto error;
    sysfont = Font_create(0);
    char *emojifontnames[] = { "Noto Color Emoji", "Noto Emoji", 0 };
    emojifont = Font_create(emojifontnames);
    if (!(win = Window_create())) goto error;

    Window_setSize(win, 640, 200);
    Window_setTitle(win, "Xmoji 😀 äöüß");
    PSC_Event_register(Window_closed(win), 0, onclose, 0);
    PSC_Event_register(Window_errored(win), 0, onclose, 0);
    return;

error:
    PSC_EAStartup_return(ea, EXIT_FAILURE);
}

static void onstartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Log_setAsync(1);
    Window_show(win);
}

static void onshutdown(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    Font_destroy(emojifont);
    emojifont = 0;
    Font_destroy(sysfont);
    sysfont = 0;
    Window_destroy(win);
    win = 0;
    Font_done();
    X11Adapter_done();
    PSC_Log_setAsync(0);
}

SOLOCAL int Xmoji_run(int argc, char **argv)
{
    startupargs.argc = argc;
    startupargs.argv = argv;

    PSC_RunOpts_foreground();
    PSC_Log_setFileLogger(stderr);
    PSC_Log_setMaxLogLevel(PSC_L_DEBUG);
    PSC_Event_register(PSC_Service_prestartup(), 0, onprestartup, 0);
    PSC_Event_register(PSC_Service_startup(), 0, onstartup, 0);
    PSC_Event_register(PSC_Service_shutdown(), 0, onshutdown, 0);
    return PSC_Service_run();
}

void *Xmoji_sysfont(void)
{
    return sysfont;
}


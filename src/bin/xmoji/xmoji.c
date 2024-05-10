#include "xmoji.h"

#include "font.h"
#include "window.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static Font *emojifont;
static Window *win;

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

    if (!(win = Window_create()))
    {
	PSC_EAStartup_return(ea, EXIT_FAILURE);
	return;
    }

    char *emojifontnames[] = { "Noto Color Emoji", "Noto Emoji", 0 };
    emojifont = Font_create(emojifontnames);

    Window_setSize(win, 640, 200);
    Window_setTitle(win, "Xmoji 😀 äöüß");
    PSC_Event_register(Window_closed(win), 0, onclose, 0);
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
    Window_destroy(win);
    X11Adapter_done();
    PSC_Log_setAsync(0);
}

SOLOCAL int Xmoji_run(int argc, char **argv)
{
    PSC_RunOpts_foreground();
    PSC_Log_setFileLogger(stderr);
    PSC_Log_setMaxLogLevel(PSC_L_DEBUG);
    PSC_Event_register(PSC_Service_prestartup(), 0, onprestartup, 0);
    PSC_Event_register(PSC_Service_startup(), 0, onstartup, 0);
    PSC_Event_register(PSC_Service_shutdown(), 0, onshutdown, 0);

    X11Adapter_init(argc, argv, "Xmoji");
    return PSC_Service_run();
}


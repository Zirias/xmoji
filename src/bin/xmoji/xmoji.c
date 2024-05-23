#include "xmoji.h"

#include "font.h"
#include "textlabel.h"
#include "valuetypes.h"
#include "window.h"
#include "x11adapter.h"

#include <inttypes.h>
#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static Font *font;
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

static const char *fontname(void)
{
    for (int i = 1; i < startupargs.argc - 1; ++i)
    {
	if (!strcmp(startupargs.argv[i], "-font"))
	{
	    return startupargs.argv[i+1];
	}
    }
    return 0;
}

static void onprestartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;

    PSC_EAStartup *ea = args;

    if (X11Adapter_init(
		startupargs.argc, startupargs.argv, "Xmoji") < 0) goto error;
    if (Font_init() < 0) goto error;
    font = Font_create(3, fontname());
    emojifont = Font_create(0, "Noto Color Emoji,Noto Emoji");
    if (!(win = Window_create(0))) goto error;
    Window_setTitle(win, "Xmoji 😀 äöüß");
    Widget_setColor(win, COLOR_BG_NORMAL, Color_fromRgb(50, 60, 70));
    Widget_setColor(win, COLOR_NORMAL, Color_fromRgb(200, 255, 240));

    TextLabel *label = TextLabel_create(win, font);
    TextLabel_setText(label, "Hello, World!\n\n"
	    "This is just a quick little\n"
	    "text rendering test.\n\n"
	    "The quick brown fox jumps over the lazy dog");
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    Window_setMainWidget(win, label);

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
    Widget_show(win);
}

static void onshutdown(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    Font_destroy(emojifont);
    emojifont = 0;
    Font_destroy(font);
    font = 0;
    Object_destroy(win);
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

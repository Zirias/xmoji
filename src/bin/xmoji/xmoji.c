#include "xmoji.h"

#include "font.h"
#include "textbox.h"
#include "textlabel.h"
#include "unistr.h"
#include "valuetypes.h"
#include "vbox.h"
#include "window.h"
#include "x11adapter.h"
#include "xrdb.h"

#include <inttypes.h>
#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static const char *fontname;
static const char *emojifontname;
static PSC_LogLevel loglevel = PSC_L_WARNING;

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

static void onprestartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;

    PSC_EAStartup *ea = args;

    if (X11Adapter_init(
		startupargs.argc, startupargs.argv, "Xmoji") < 0) goto error;
    XRdb *res = X11Adapter_resources();
    if (res)
    {
	if (!fontname) fontname = XRdb_value(res, XRdbKey("font"));
	if (!emojifontname) emojifontname = XRdb_value(
		res, XRdbKey("emojifont"));
    }
    if (!emojifontname) emojifontname = "emoji";
    if (Font_init(.15) < 0) goto error;
    font = Font_create(3, fontname);
    emojifont = Font_create(0, emojifontname);
    if (!(win = Window_create(0))) goto error;
    Window_setTitle(win, "Xmoji 😀 äöüß");
    Widget_setColor(win, COLOR_BG_NORMAL, Color_fromRgb(50, 60, 70));
    Widget_setColor(win, COLOR_NORMAL, Color_fromRgb(200, 255, 240));
    Widget_setColor(win, COLOR_BG_SELECTED, Color_fromRgb(100, 200, 255));
    Widget_setColor(win, COLOR_SELECTED, Color_fromRgb(0, 0, 0));
    Widget_setColor(win, COLOR_BG_ACTIVE, Color_fromRgb(40, 50, 60));
    Widget_setColor(win, COLOR_DISABLED, Color_fromRgb(120, 120, 120));

    VBox *box = VBox_create(win);

    TextLabel *label = TextLabel_create(box, font);
    UniStr(hello, "Hello, World!\n\n"
	    "This is just a quick little\n"
	    "text rendering test.\n\n"
	    "The quick brown fox jumps over the lazy dog");
    TextLabel_setText(label, hello);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    TextBox *input = TextBox_create(box, font);
    UniStr(clickhere, "Click here to type ...");
    TextBox_setPlaceholder(input, clickhere);
    Widget_show(input);
    VBox_addWidget(box, input);

    label = TextLabel_create(box, emojifont);
    UniStr(emojis, "😀🤡🇩🇪👺🧩🔮🐅🍻🧑🏾‍🤝‍🧑🏻");
    TextLabel_setText(label, emojis);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    input = TextBox_create(box, font);
    TextBox_setPlaceholder(input, clickhere);
    Widget_show(input);
    VBox_addWidget(box, input);

    Widget_show(box);
    Window_setMainWidget(win, box);

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

#ifndef DEBUG
    PSC_Log_setAsync(1);
#endif
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

    for (int i = 1; i < argc; ++i)
    {
	if (i < argc - 1 && !strcmp(argv[i], "-font"))
	{
	    fontname = argv[i+1];
	}
	else if (i < argc - 1 && !strcmp(argv[i], "-emojifont"))
	{
	    emojifontname = argv[i+1];
	}
	else if (loglevel < PSC_L_INFO && !strcmp(argv[i], "-v"))
	{
	    loglevel = PSC_L_INFO;
	}
	else if (loglevel < PSC_L_DEBUG && !strcmp(argv[i], "-vv"))
	{
	    loglevel = PSC_L_DEBUG;
	}
    }

    PSC_RunOpts_foreground();
    PSC_Log_setFileLogger(stderr);
    PSC_Log_setMaxLogLevel(loglevel);
    PSC_Event_register(PSC_Service_prestartup(), 0, onprestartup, 0);
    PSC_Event_register(PSC_Service_startup(), 0, onstartup, 0);
    PSC_Event_register(PSC_Service_shutdown(), 0, onshutdown, 0);
    return PSC_Service_run();
}

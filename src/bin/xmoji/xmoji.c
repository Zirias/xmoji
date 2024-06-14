#include "xmoji.h"

#include "button.h"
#include "scrollbox.h"
#include "textbox.h"
#include "textlabel.h"
#include "unistr.h"
#include "valuetypes.h"
#include "vbox.h"
#include "window.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>

static PSC_LogLevel loglevel = PSC_L_WARNING;

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

    if (!(win = Window_create("mainWindow", 0))) goto error;
    Window_setTitle(win, "Xmoji 😀 äöüß");

    ScrollBox *scroll = ScrollBox_create("mainScrollBox", win);
    VBox *box = VBox_create(scroll);

    TextLabel *label = TextLabel_create("helloLabel", box);
    UniStr(hello, "Hello, World!\n\n"
	    "This is just a quick little\n"
	    "text rendering test.\n\n"
	    "The quick brown fox jumps over the lazy dog");
    TextLabel_setText(label, hello);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    TextBox *input = TextBox_create("upperBox", box);
    UniStr(clickhere, "Click here to type ...");
    TextBox_setPlaceholder(input, clickhere);
    Widget_show(input);
    VBox_addWidget(box, input);

    Button *button = Button_create("quitButton", box);
    UniStr(quit, "Quit");
    Button_setText(button, quit);
    Widget_setAlign(button, AH_CENTER);
    Widget_show(button);
    VBox_addWidget(box, button);

    label = TextLabel_create("emojiLabel", box);
    Widget_setFontResName(label, "emojifont", "emoji", 0);
    UniStr(emojis, "😀🤡🇩🇪👺🧩🔮🐅🍻🧑🏾‍🤝‍🧑🏻");
    TextLabel_setText(label, emojis);
    Widget_setAlign(label, AH_CENTER|AV_MIDDLE);
    Widget_show(label);
    VBox_addWidget(box, label);

    input = TextBox_create("lowerBox", box);
    TextBox_setPlaceholder(input, clickhere);
    UniStr(prefilled, "This textbox is prefilled!");
    TextBox_setText(input, prefilled);
    Widget_show(input);
    VBox_addWidget(box, input);

    Widget_show(box);
    ScrollBox_setWidget(scroll, box);

    Widget_show(scroll);
    Window_setMainWidget(win, scroll);

    PSC_Event_register(Window_closed(win), 0, onclose, 0);
    PSC_Event_register(Button_clicked(button), 0, onclose, 0);
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

    Object_destroy(win);
    win = 0;
    X11Adapter_done();
    PSC_Log_setAsync(0);
}

SOLOCAL int Xmoji_run(int argc, char **argv)
{
    startupargs.argc = argc;
    startupargs.argv = argv;

    for (int i = 1; i < argc; ++i)
    {
	if (loglevel < PSC_L_INFO && !strcmp(argv[i], "-v"))
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

#include "xmoji.h"

#include "x11adapter.h"
#include "mainwindow.h"

#include <poser/core.h>
#include <stdlib.h>

static X11Adapter *x11;
static MainWindow *win;

static void onclose(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Service_quit();
}

static void onstartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    MainWindow_show(win);
}

static void onshutdown(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;
}

SOLOCAL int Xmoji_run(void)
{
    int rc = EXIT_FAILURE;
    x11 = X11Adapter_create();
    if (!x11) goto done;
    win = MainWindow_create(x11);
    if (!win) goto done;

    PSC_RunOpts_init(0);
    PSC_RunOpts_foreground();
    PSC_Event_register(PSC_Service_startup(), 0, onstartup, 0);
    PSC_Event_register(PSC_Service_shutdown(), 0, onshutdown, 0);
    PSC_Event_register(MainWindow_closed(win), 0, onclose, 0);
    rc = PSC_Service_run();

done:
    MainWindow_destroy(win);
    X11Adapter_destroy(x11);
    return rc;
}


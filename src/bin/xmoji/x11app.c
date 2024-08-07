#include "x11app.h"

#include "widget.h"
#include "window.h"
#include "x11adapter.h"

#include <locale.h>
#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void destroy(void *obj);

static X11App *instance;
static MetaX11App mo = MetaX11App_init(0, 0, "X11App", destroy);

struct X11App
{
    Object base;
    PSC_List *windows;
    PSC_Event *error;
    char *locale;
    char *name;
    char **argv;
    int argc;
    int quitting;
};

struct X11Error
{
    Window *window;
    Widget *widget;
    int ignore;
    uint16_t opMinor;
    uint8_t opMajor;
    uint8_t code;
};

static void svprestartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Service_setTickInterval(0);
}

static void svstartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;

#ifndef DEBUG
    PSC_Log_setAsync(1);
#endif
    PSC_EAStartup *ea = args;
    int rc = 0;
    Object_vcall(rc, X11App, startup, instance);
    if (rc != 0) PSC_EAStartup_return(ea, EXIT_FAILURE);
}

static void svshutdown(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    X11App *self = receiver;
#ifndef DEBUG
    PSC_Log_setAsync(0);
#endif
    Object_vcallv(X11App, shutdown, self);
    if (PSC_List_size(self->windows))
    {
	PSC_Service_setTickInterval(1000);
	PSC_Service_shutdownLock();
	self->quitting = 1;
	PSC_ListIterator *i = PSC_List_iterator(self->windows);
	while (PSC_ListIterator_moveNext(i))
	{
	    Window *win = PSC_ListIterator_current(i);
	    Window_close(win);
	}
	PSC_ListIterator_destroy(i);
    }
}

static void destroy(void *obj)
{
    X11Adapter_done();
    X11App *self = obj;
    PSC_Event_unregister(PSC_Service_shutdown(), self, svshutdown, 0);
    PSC_Event_unregister(PSC_Service_startup(), self, svstartup, 0);
    PSC_Event_unregister(PSC_Service_prestartup(), self, svprestartup, 0);
    free(self->name);
    free(self->locale);
    PSC_Event_destroy(self->error);
    PSC_List_destroy(self->windows);
    free(self);
    instance = 0;
}

static char *getLocale(void)
{
    char *lc = setlocale(LC_ALL, "");
    if (!lc) lc = "C";
    char *lcdot = strchr(lc, '.');
    if (!lcdot || strcmp(lcdot+1, "UTF-8"))
    {
	char utf8lc[32];
	if (lcdot) *lcdot = 0;
	snprintf(utf8lc, sizeof utf8lc, "%s.UTF-8", lc);
	PSC_Log_fmt(PSC_L_WARNING, "Configured locale doesn't use UTF-8 "
		"encoding, trying `%s' instead", utf8lc);
	lc = setlocale(LC_ALL, utf8lc);
    }
    if (lc)
    {
	PSC_Log_fmt(PSC_L_INFO, "Starting with locale `%s'", lc);
	return PSC_copystr(lc);
    }
    PSC_Log_msg(PSC_L_ERROR, "Couldn't set locale");
    return 0;
}

static void initPoser(int argc, char **argv)
{
    PSC_LogLevel loglevel = PSC_L_WARNING;
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
}

static char *getName(int argc, char **argv)
{
    for (int i = 1; i < argc-1; ++i)
    {
	if (!strcmp(argv[i], "-name"))
	{
	    return argv[++i];
	}
    }
    char *nm = getenv("RESOURCE_NAME");
    if (nm) return nm;
    if (argv[0] && *argv[0])
    {
	char *lastslash = strrchr(argv[0], '/');
	if (lastslash) nm = lastslash+1;
	else nm = argv[0];
    }
    return PSC_copystr(*nm ? nm : "x11app");
}

X11App *app(void)
{
    return instance;
}

X11App *X11App_createBase(void *derived, int argc, char **argv)
{
    if (instance) return 0;
    char *locale = getLocale();
    if (!locale) return 0;
    initPoser(argc, argv);

    X11App *self = PSC_malloc(sizeof *self);
    CREATEBASE(Object);
    self->windows = PSC_List_create();
    self->error = PSC_Event_create(self);
    self->locale = locale;
    self->name = getName(argc, argv);
    self->argv = argv;
    self->argc = argc;
    self->quitting = 0;

    PSC_Event_register(PSC_Service_prestartup(), self, svprestartup, 0);
    PSC_Event_register(PSC_Service_startup(), self, svstartup, 0);
    PSC_Event_register(PSC_Service_shutdown(), self, svshutdown, 0);

    return (instance = self);
}

int X11App_run(void)
{
    if (!instance) return EXIT_FAILURE;
    int rc = X11Adapter_init(instance->argc, instance->argv, instance->locale,
	    instance->name, Object_className(instance));
    if (rc == 0) rc = PSC_Service_run();
    Object_destroy(instance);
    return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}

void X11App_quit(void)
{
    if (!instance) return;
    PSC_Service_quit();
}

PSC_Event *X11App_error(void)
{
    if (!instance) return 0;
    return instance->error;
}

void X11App_raiseError(X11App *self, Window *window, Widget *widget,
	RequestErrorEventArgs *err)
{
    X11Error error = {
	.window = window,
	.widget = widget,
	.ignore = 0,
	.opMinor = err->opMinor,
	.opMajor = err->opMajor,
	.code = err->code
    };
    PSC_Event_raise(self->error, 0, &error);
    if (error.ignore) return;
    PSC_Log_setAsync(0);
    const char *widgetName = Widget_name(widget);
    if (!widgetName) widgetName = "<unnamed>";
    PSC_Log_fmt(PSC_L_ERROR, "Unhandled X11 error from %s `%s' in "
	    "Window `%s' (0x%x) for opcode %u:%u, error code %u - exiting.",
	    Object_className(widget), widgetName,
	    Widget_resname(window), Window_id(window),
	    (unsigned)err->opMajor, (unsigned)err->opMinor,
	    (unsigned)err->code);
#ifdef TRACE_X11_REQUESTS
    PSC_Log_fmt(PSC_L_ERROR, "** TRACE:  In %s(), %s:%u",
	    err->reqid.function, err->reqid.sourcefile, err->reqid.lineno);
    PSC_Log_fmt(PSC_L_ERROR, "** FAILED: %s", err->reqid.reqsource);
#endif
    X11App_quit();
}

void X11App_addWindow(X11App *self, Window *window)
{
    PSC_List_remove(self->windows, window);
    PSC_List_append(self->windows, window, 0);
}

void X11App_removeWindow(X11App *self, Window *window)
{
    PSC_List_remove(self->windows, window);
    if (self->quitting && !PSC_List_size(self->windows))
    {
	PSC_Service_shutdownUnlock();
    }
}

Window *X11Error_window(X11Error *self)
{
    return self->window;
}

Widget *X11Error_widget(X11Error *self)
{
    return self->widget;
}

uint8_t X11Error_code(X11Error *self)
{
    return self->code;
}

uint8_t X11Error_opMajor(X11Error *self)
{
    return self->opMajor;
}

uint16_t X11Error_opMinor(X11Error *self)
{
    return self->opMinor;
}

void X11Error_ignore(X11Error *self)
{
    self->ignore = 1;
}

void X11App_showWaitCursor(void)
{
    if (!instance) return;
    PSC_ListIterator *i = PSC_List_iterator(instance->windows);
    while (PSC_ListIterator_moveNext(i))
    {
	Window *win = PSC_ListIterator_current(i);
	Window_showWaitCursor(win);
    }
    PSC_ListIterator_destroy(i);
}

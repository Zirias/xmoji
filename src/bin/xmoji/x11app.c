#define _XOPEN_SOURCE 600
/* #define _POSIX_C_SOURCE 200112L
 * for gethostname() on NetBSD, where it's only exposed as part of XPG4.2,
 * although specified in POSIX since POSIX.1-2001
 */

#include "x11app.h"

#include "suppress.h"
#include "widget.h"
#include "window.h"
#include "x11adapter.h"

#include <locale.h>
#include <poser/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void destroy(void *obj);

static X11App *instance;
static MetaX11App mo = MetaX11App_init(0, 0, "X11App", destroy);

typedef struct AppLocale
{
    char *lc_ctype;
    char *lc_messages;
} AppLocale;

struct X11App
{
    Object base;
    PSC_List *windows;
    PSC_Event *error;
    AppLocale locale;
    char *name;
    char **argv;
    char *hostprop;
    char *cmdprop;
    size_t hostproplen;
    size_t cmdproplen;
    uint32_t pidprop;
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
    free(self->cmdprop);
    free(self->hostprop);
    free(self->name);
    free(self->locale.lc_messages);
    free(self->locale.lc_ctype);
    PSC_Event_destroy(self->error);
    PSC_List_destroy(self->windows);
    free(self);
    instance = 0;
}

static int getLocale(AppLocale *locale)
{
    int rc = -1;
    if (!setlocale(LC_ALL, ""))
    {
	PSC_Log_msg(PSC_L_ERROR, "Couldn't set locale: setlocale() failed.");
	goto done;
    }
    char *lc_ctype = setlocale(LC_CTYPE, 0);
    if (!lc_ctype) lc_ctype = "C";
    char *lcdot = strchr(lc_ctype, '.');
    if (!lcdot || strcmp(lcdot+1, "UTF-8"))
    {
	char utf8lc[32];
	if (lcdot) *lcdot = 0;
	snprintf(utf8lc, sizeof utf8lc, "%s.UTF-8", lc_ctype);
	PSC_Log_fmt(PSC_L_WARNING, "Configured LC_CTYPE doesn't use UTF-8 "
		"encoding, trying `%s' instead", utf8lc);
	lc_ctype = setlocale(LC_CTYPE, utf8lc);
    }
    if (lc_ctype && strstr(lc_ctype, "UTF-8"))
    {
	locale->lc_ctype = PSC_copystr(lc_ctype);
	char *lc_messages = setlocale(LC_MESSAGES, 0);
	if (!lc_messages) lc_messages = "C";
	locale->lc_messages = PSC_copystr(lc_messages);
	PSC_Log_fmt(PSC_L_INFO,
		"Starting with locale: LC_CTYPE=%s LC_MESSAGES=%s",
		locale->lc_ctype, locale->lc_messages);
	rc = 0;
    }
    else PSC_Log_msg(PSC_L_ERROR,
	    "Couldn't set locale: failed to set LC_CTYPE to UTF-8 encoding");
done:
    return rc;
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
    initPoser(argc, argv);
    AppLocale locale;
    if (getLocale(&locale) < 0) return 0;

    X11App *self = PSC_malloc(sizeof *self);
    CREATEBASE(Object);
    self->windows = PSC_List_create();
    self->error = PSC_Event_create(self);
    self->locale = locale;
    self->name = getName(argc, argv);
    self->argv = argv;
    self->hostprop = 0;
    self->cmdprop = 0;
    self->hostproplen = 0;
    self->cmdproplen = 0;
    self->pidprop = 0;
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
    int rc = X11Adapter_init(instance->argc, instance->argv,
	    instance->locale.lc_ctype, instance->name,
	    Object_className(instance));
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

void X11App_setWmProperties(Window *win)
{
    if (!instance) return;
    if (!instance->hostprop)
    {
	long maxlen = sysconf(_SC_HOST_NAME_MAX);
	if (maxlen < 0) maxlen = 1024;
	instance->hostprop = PSC_malloc(maxlen + 1);
	gethostname(instance->hostprop, maxlen + 1);
	instance->hostprop[maxlen] = 0;
	instance->hostproplen = strlen(instance->hostprop);
	if (instance->hostproplen < (size_t)maxlen)
	{
	    instance->hostprop = PSC_realloc(instance->hostprop,
		    instance->hostproplen + 1);
	}
	instance->cmdproplen = 0;
	for (int i = 0; i < instance->argc; ++i)
	{
	    instance->cmdproplen += strlen(instance->argv[i]) + 1;
	}
	instance->cmdprop = PSC_malloc(instance->cmdproplen);
	size_t pos = 0;
	for (int i = 0; i < instance->argc; ++i)
	{
	    size_t argvlen = strlen(instance->argv[i]) + 1;
	    memcpy(instance->cmdprop + pos, instance->argv[i], argvlen);
	    pos += argvlen;
	}
	instance->pidprop = getpid();
    }
    xcb_connection_t *c = X11Adapter_connection();
    xcb_window_t w = Window_id(win);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		XCB_ATOM_WM_CLIENT_MACHINE, XCB_ATOM_STRING, 8,
		instance->hostproplen, instance->hostprop),
	    "Cannot set WM_CLIENT_MACHINE for 0x%x", (unsigned)w);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		XCB_ATOM_WM_COMMAND, XCB_ATOM_STRING, 8,
		instance->cmdproplen, instance->cmdprop),
	    "Cannot set WM_COMMAND for 0x%x", (unsigned)w);
    CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, w,
		A(_NET_WM_PID), XCB_ATOM_CARDINAL, 32, 1, &instance->pidprop),
	    "Cannot set _NET_WM_PID for 0x%x", (unsigned)w);
}

const char *X11App_lcMessages(void)
{
    if (!instance) return 0;
    return instance->locale.lc_messages;
}

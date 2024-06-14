#include "x11app.h"

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
    char *locale;
    char *name;
    char **argv;
    int argc;
};

static void svstartup(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

#ifndef DEBUG
    PSC_Log_setAsync(1);
#endif
}

static void svshutdown(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

#ifndef DEBUG
    PSC_Log_setAsync(0);
#endif
}

static void destroy(void *obj)
{
    X11Adapter_done();
    X11App *self = obj;
    PSC_Event_unregister(PSC_Service_shutdown(), self, svshutdown, 0);
    PSC_Event_unregister(PSC_Service_startup(), self, svstartup, 0);
    free(self->name);
    free(self->locale);
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
    REGTYPE(0);

    X11App *self = PSC_malloc(sizeof *self);
    if (!derived) derived = self;
    self->base.type = OBJTYPE;
    self->base.base = Object_create(derived);
    self->locale = locale;
    self->name = getName(argc, argv);
    self->argv = argv;
    self->argc = argc;

#ifndef DEBUG
    PSC_Event_register(PSC_Service_startup(), self, svstartup, 0);
#endif
    PSC_Event_register(PSC_Service_shutdown(), self, svshutdown, 0);

    return (instance = self);
}

int X11App_run(void)
{
    if (!instance) return EXIT_FAILURE;
    int rc = X11Adapter_init(instance->argc, instance->argv, instance->locale,
	    instance->name, Object_className(instance));
    if (rc != 0) goto done;
    Object_vcall(rc, X11App, startup, instance);
    if (rc != 0) goto done;
    rc = PSC_Service_run();
    Object_vcallv(X11App, shutdown, instance);
done:
    Object_destroy(instance);
    return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}


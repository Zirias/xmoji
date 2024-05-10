#include "x11adapter.h"

#include <locale.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <xcb/xcbext.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define X_SZNM(a) { sizeof STR(a) - 1, STR(a) },

typedef struct X11ReplyHandler
{
    void (*handler)(void *ctx, void *reply, xcb_generic_error_t *error);
    void *ctx;
    xcb_void_cookie_t cookie;
} X11ReplyHandler;

static const struct { int len; const char *nm; } atomnm[] = {
    X_ATOMS(X_SZNM)
};

static char wmclass[512];
static size_t wmclasssz;

static xcb_connection_t *c;
static xcb_screen_t *s;
static PSC_Event *clientmsg;
static X11ReplyHandler *nextReply;
static PSC_Queue *waitingReplies;
static xcb_atom_t atoms[NATOMS];
static int fd;

static X11ReplyHandler *X11ReplyHandler_create(void *cookie, void *ctx,
	void (*handler)(void *, void *, xcb_generic_error_t *))
{
    X11ReplyHandler *self = PSC_malloc(sizeof *self);
    self->handler = handler;
    self->ctx = ctx;
    self->cookie.sequence = ((xcb_void_cookie_t *)cookie)->sequence;
    return self;
}

static void readEvents(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    xcb_generic_event_t *ev = 0;

    if (nextReply)
    {
	void *reply = 0;
        xcb_generic_error_t *error = 0;

	while (xcb_poll_for_reply(c, nextReply->cookie.sequence,
		    &reply, &error))
	{
	    nextReply->handler(nextReply->ctx, reply, error);
	    free(reply);
	    free(error);
	    free(nextReply);
	    nextReply = PSC_Queue_dequeue(waitingReplies);
	    if (!nextReply) break;
	}

	ev = xcb_poll_for_queued_event(c);
    }
    else ev = xcb_poll_for_event(c);

    while (ev)
    {
	switch (ev->response_type & 0x7f)
	{
	    case XCB_CLIENT_MESSAGE:
		PSC_Event_raise(clientmsg,
			((xcb_client_message_event_t *)ev)->window, ev);
		break;

	    default:
		break;
	}

	free(ev);
	ev = xcb_poll_for_queued_event(c);
    }

    xcb_flush(c);
}

static void X11Adapter_connect(void)
{
    if (c)
    {
	if (xcb_connection_has_error(c))
	{
	    PSC_Log_setAsync(0);
	    PSC_Log_msg(PSC_L_ERROR, "Lost X11 connection.");
	    PSC_Service_quit();
	}
	return;
    }
    c = xcb_connect(0, 0);
    if (xcb_connection_has_error(c))
    {
	xcb_disconnect(c);
	c = 0;
	PSC_Log_msg(PSC_L_ERROR,
		"Error connecting to X server. This program requires X11. "
		"If you're in an X session, check your DISPLAY variable.");
	return;
    }

    s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    clientmsg = PSC_Event_create(0);
    nextReply = 0;
    waitingReplies = PSC_Queue_create();
    fd = xcb_get_file_descriptor(c);

    xcb_intern_atom_cookie_t atomcookies[NATOMS];
    for (int i = 0; i < NATOMS; ++i)
    {
	atomcookies[i] = xcb_intern_atom(c, 0, atomnm[i].len, atomnm[i].nm);
    }
    for (int i = 0; i < NATOMS; ++i)
    {
	xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(c,
		atomcookies[i], 0);
	if (atom)
	{
	    atoms[i] = atom->atom;
	    free(atom);
	}
	else atoms[i] = 0;
    }

    PSC_Event_register(PSC_Service_readyRead(), 0, readEvents, fd);
    PSC_Service_registerRead(fd);
}

void X11Adapter_init(int argc, char **argv, const char *classname)
{
    char *lc = setlocale(LC_ALL, "");
    if (!lc) lc = "C";
    char *lcdot = strchr(lc, '.');
    if (!lcdot || strcmp(lcdot+1, "UTF-8"))
    {
	char utf8lc[32];
	if (lcdot) *lcdot = 0;
	snprintf(utf8lc, sizeof utf8lc, "%s.UTF-8", lc);
	PSC_Log_fmt(PSC_L_INFO, "Configured locale doesn't use UTF-8 "
		"encoding, trying `%s' instead", utf8lc);
	lc = setlocale(LC_ALL, utf8lc);
    }
    if (lc)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Starting with locale `%s'", lc);
    }
    else
    {
	PSC_Log_msg(PSC_L_WARNING,
		"Couldn't set locale, problems might occur");
    }

    char *nm = 0;
    for (int i = 1; i < argc-1; ++i)
    {
	if (!strcmp(argv[i], "-name"))
	{
	    nm = argv[++i];
	    break;
	}
    }
    if (!nm) nm = getenv("RESOURCE_NAME");
    if (!nm && argv[0] && *argv[0])
    {
	char *lastslash = strrchr(argv[0], '/');
	if (lastslash) nm = lastslash+1;
	else nm = argv[0];
	if (!*nm) nm = 0;
    }
    if (!nm) nm = "unknown";
    nm = X11Adapter_toLatin1(nm);
    char *clnm = 0;
    if (classname)
    {
	clnm = X11Adapter_toLatin1(classname);
	classname = clnm;
    }
    else classname = nm;
    int len = snprintf(wmclass, sizeof wmclass / 2, "%s", nm);
    size_t pos = len + 1;
    if (pos > sizeof wmclass / 2) pos = sizeof wmclass / 2;
    int len2 = snprintf(wmclass+pos, sizeof wmclass / 2, "%s", classname);
    wmclasssz = pos + len2 + 1;
    if (wmclasssz > sizeof wmclass) wmclasssz = sizeof wmclass;
    PSC_Log_fmt(PSC_L_DEBUG,
	    "starting with window class \"%s\", \"%s\"", nm, classname);
    free(clnm);
    free(nm);
}

xcb_connection_t *X11Adapter_connection(void)
{
    X11Adapter_connect();
    return c;
}

xcb_screen_t *X11Adapter_screen(void)
{
    return s;
}

xcb_atom_t X11Adapter_atom(XAtomId id)
{
    return atoms[id];
}

PSC_Event *X11Adapter_clientmsg(void)
{
    return clientmsg;
}

const char *X11Adapter_wmClass(size_t *sz)
{
    if (sz) *sz = wmclasssz;
    return wmclass;
}

void X11Adapter_await(void *cookie, void *ctx,
	void (*handler)(void *ctx, void *reply, xcb_generic_error_t *error))
{
    X11ReplyHandler *rh = X11ReplyHandler_create(cookie, ctx, handler);
    if (nextReply) PSC_Queue_enqueue(waitingReplies, rh, free);
    else nextReply = rh;
}

char *X11Adapter_toLatin1(const char *utf8)
{
    size_t len = strlen(utf8);
    char *latin1 = PSC_malloc(len+1);

    const unsigned char *in = (const unsigned char *)utf8;
    char *out = latin1;
    for (; *in; ++in, ++out)
    {
	if (*in < 0x80)
	{
	    *out = *in;
	    continue;
	}
	char32_t uc = 0;
	int f = 0;
	if ((*in & 0xe0) == 0xc0)
	{
	    uc = (*in & 0x1f);
	    f = 1;
	}
	else if ((*in & 0xf0) == 0xe0)
	{
	    uc = (*in & 0xf);
	    f = 2;
	}
	else if ((*in & 0xf8) == 0xf0)
	{
	    uc = (*in & 0x7);
	    f = 3;
	}
	else
	{
	    *out = '?';
	    continue;
	}
	for (; f && *++in; --f)
	{
	    if ((*in & 0xc0) != 0x80) break;
	    uc <<= 6;
	    uc |= (*in & 0x3f);
	}
	if (f || uc > 0xff) *out = '?';
	else *out = uc;
    }

    return latin1;
}

void X11Adapter_done(void)
{
    if (!c) return;
    PSC_Service_unregisterRead(fd);
    PSC_Event_unregister(PSC_Service_readyRead(), 0, readEvents, fd);
    fd = 0;
    PSC_Queue_destroy(waitingReplies);
    waitingReplies = 0;
    free(nextReply);
    nextReply = 0;
    PSC_Event_destroy(clientmsg);
    clientmsg = 0;
    xcb_disconnect(c);
    c = 0;
    s = 0;
    memset(atoms, 0, sizeof atoms);
}


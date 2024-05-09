#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
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

void X11Adapter_await(void *cookie, void *ctx,
	void (*handler)(void *ctx, void *reply, xcb_generic_error_t *error))
{
    X11ReplyHandler *rh = X11ReplyHandler_create(cookie, ctx, handler);
    if (nextReply) PSC_Queue_enqueue(waitingReplies, rh, free);
    else nextReply = rh;
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


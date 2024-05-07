#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define X_SZNM(a) { sizeof STR(a) - 1, STR(a) },

static const struct { int len; const char *nm; } atomnm[] = {
    X_ATOMS(X_SZNM)
};

struct X11Adapter
{
    xcb_connection_t *c;
    xcb_screen_t *s;
    PSC_Event *clientmsg;
    xcb_atom_t atoms[NATOMS];
    int fd;
};

static void readEvents(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    X11Adapter *self = receiver;
    xcb_generic_event_t *ev;
    while ((ev = xcb_poll_for_event(self->c)))
    {
	switch (ev->response_type & 0x7f)
	{
	    case XCB_CLIENT_MESSAGE:
		PSC_Event_raise(self->clientmsg,
			((xcb_client_message_event_t *)ev)->window, ev);
		break;

	    default:
		break;
	}

	free(ev);
    }
    xcb_flush(self->c);
}

SOLOCAL X11Adapter *X11Adapter_create(void)
{
    xcb_connection_t *c = xcb_connect(0, 0);
    if (!c) return 0;

    X11Adapter *self = PSC_malloc(sizeof *self);
    self->c = c;
    self->s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    self->clientmsg = PSC_Event_create(self);
    self->fd = xcb_get_file_descriptor(c);

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
	    self->atoms[i] = atom->atom;
	    free(atom);
	}
	else self->atoms[i] = 0;
    }

    PSC_Event_register(PSC_Service_readyRead(), self, readEvents, self->fd);
    PSC_Service_registerRead(self->fd);

    return self;
}

xcb_connection_t *X11Adapter_connection(X11Adapter *self)
{
    return self->c;
}

xcb_screen_t *X11Adapter_screen(X11Adapter *self)
{
    return self->s;
}

xcb_atom_t X11Adapter_atom(X11Adapter *self, XAtomId id)
{
    return self->atoms[id];
}

PSC_Event *X11Adapter_clientmsg(X11Adapter *self)
{
    return self->clientmsg;
}

void X11Adapter_destroy(X11Adapter *self)
{
    if (!self) return;

    PSC_Service_unregisterRead(self->fd);
    PSC_Event_unregister(PSC_Service_readyRead(), self, readEvents, self->fd);
    PSC_Event_destroy(self->clientmsg);
    xcb_disconnect(self->c);
    free(self);
}


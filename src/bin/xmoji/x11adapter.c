#include "x11adapter.h"

#include <inttypes.h>
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
static xcb_render_pictformat_t rootformat;
static xcb_render_pictformat_t alphaformat;
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

int X11Adapter_init(int argc, char **argv, const char *classname)
{
    xcb_render_query_pict_formats_reply_t *pf = 0;
    if (c) return 0;
    c = xcb_connect(0, 0);
    if (xcb_connection_has_error(c))
    {
	PSC_Log_msg(PSC_L_ERROR,
		"Could not connect to X server. This program requires X11. "
		"If you're in an X session, check your DISPLAY variable.");
	goto error;
    }

    xcb_render_query_version_cookie_t versioncookie =
	xcb_render_query_version(c,
		XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);
    xcb_intern_atom_cookie_t atomcookies[NATOMS];
    for (int i = 0; i < NATOMS; ++i)
    {
	atomcookies[i] = xcb_intern_atom(c, 0, atomnm[i].len, atomnm[i].nm);
    }
    xcb_render_query_version_reply_t *version =
	xcb_render_query_version_reply(c, versioncookie, 0);
    xcb_render_query_pict_formats_cookie_t formatscookie;
    if (version)
    {
	formatscookie = xcb_render_query_pict_formats(c);
	PSC_Log_fmt(PSC_L_DEBUG, "using XRender version %"PRIu32".%"PRIu32,
		version->major_version, version->minor_version);
	free(version);
    }
    else goto error;
    for (int i = 0; i < NATOMS; ++i)
    {
	xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(c,
		atomcookies[i], 0);
	if (atom)
	{
	    atoms[i] = atom->atom;
	    free(atom);
	}
	else
	{
	    PSC_Log_fmt(PSC_L_ERROR, "Could not find or create X atom `%s'",
		    atomnm[i].nm);
	    goto error;
	}
    }
    pf = xcb_render_query_pict_formats_reply(c, formatscookie, 0);
    if (!pf)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not query picture formats");
	goto error;
    }

    s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_render_pictscreen_iterator_t si =
	xcb_render_query_pict_formats_screens_iterator(pf);
    while (!rootformat && si.rem)
    {
	xcb_render_pictdepth_iterator_t di =
	    xcb_render_pictscreen_depths_iterator(si.data);
	while (!rootformat && di.rem)
	{
	    xcb_render_pictvisual_iterator_t vi =
		xcb_render_pictdepth_visuals_iterator(di.data);
	    while (!rootformat && vi.rem)
	    {
		if (vi.data->visual == s->root_visual)
		{
		    rootformat = vi.data->format;
		}
		xcb_render_pictvisual_next(&vi);
	    }
	    xcb_render_pictdepth_next(&di);
	}
	xcb_render_pictscreen_next(&si);
    }
    for (xcb_render_pictforminfo_iterator_t fi =
	    xcb_render_query_pict_formats_formats_iterator(pf);
	    fi.rem;
	    xcb_render_pictforminfo_next(&fi))
    {
	if (fi.data->id == rootformat)
	{
	    if (fi.data->type == XCB_RENDER_PICT_TYPE_DIRECT
		    && fi.data->depth >= 24)
	    {
		PSC_Log_fmt(PSC_L_DEBUG,
			"Root visual picture format ok (DIRECT, %"PRIu32"bpp)",
			fi.data->depth);
	    }
	    else
	    {
		PSC_Log_fmt(PSC_L_ERROR,
			"Incompatible root visual format: %s, %"PRIu32"bpp",
			fi.data->type == XCB_RENDER_PICT_TYPE_DIRECT ?
			"DIRECT" : "INDEXED", fi.data->depth);
		goto error;
	    }
	}
	if (fi.data->type != XCB_RENDER_PICT_TYPE_DIRECT) continue;
	if (fi.data->depth != 8) continue;
	if (fi.data->direct.red_mask != 0) continue;
	if (fi.data->direct.green_mask != 0) continue;
	if (fi.data->direct.blue_mask != 0) continue;
	if (fi.data->direct.alpha_mask != 255) continue;
	alphaformat = fi.data->id;
    }
    if (!alphaformat)
    {
	PSC_Log_msg(PSC_L_ERROR, "No 8bit alpha format found");
	goto error;
    }
    free(pf);
    pf = 0;

    clientmsg = PSC_Event_create(0);
    nextReply = 0;
    waitingReplies = PSC_Queue_create();
    fd = xcb_get_file_descriptor(c);
    PSC_Event_register(PSC_Service_readyRead(), 0, readEvents, fd);
    PSC_Service_registerRead(fd);

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

    return 0;

error:
    free(pf);
    xcb_disconnect(c);
    c = 0;
    s = 0;
    return -1;
}

xcb_connection_t *X11Adapter_connection(void)
{
    if (xcb_connection_has_error(c))
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR, "Lost X11 connection.");
	PSC_Service_quit();
    }
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

xcb_render_pictformat_t X11Adapter_rootformat(void)
{
    return rootformat;
}

xcb_render_pictformat_t X11Adapter_alphaformat(void)
{
    return alphaformat;
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
    rootformat = 0;
    alphaformat = 0;
    xcb_disconnect(c);
    c = 0;
    s = 0;
    memset(atoms, 0, sizeof atoms);
}


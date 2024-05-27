#include "x11adapter.h"

#include <inttypes.h>
#include <locale.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <xcb/xcbext.h>

#define X_SZNM(a) { sizeof STR(a) - 1, STR(a) },

#define MAXWAITING 128

#define RQ_AWAIT_REPLY 1
#define RQ_AWAIT_NOREPLY 0
#define RQ_CHECK_ERROR -1
#define RQ_CHECK_ERROR_UNSIGNED -2
#define RQ_CHECK_ERROR_STRING -3

typedef struct X11ReplyHandlerRecord
{
    X11ReplyHandler handler;
    void *ctx;
    unsigned sequence;
    int replytype;
    const char *sarg;
    unsigned uarg;
} X11ReplyHandlerRecord;

static const struct { int len; const char *nm; } atomnm[] = {
    X_ATOMS(X_SZNM)
};

static char wmclass[512];
static size_t wmclasssz;

static xcb_connection_t *c;
static xcb_screen_t *s;
static size_t maxRequestSize;
static PSC_Event *clientmsg;
static PSC_Event *configureNotify;
static PSC_Event *expose;
static PSC_Event *mapNotify;
static PSC_Event *unmapNotify;
static PSC_Event *requestError;
static PSC_Event *eventsDone;
static xcb_atom_t atoms[NATOMS];
static xcb_render_pictformat_t rootformat;
static xcb_render_pictformat_t alphaformat;
static xcb_render_pictformat_t argbformat;
static int fd;

static X11ReplyHandlerRecord waitingReplies[MAXWAITING];
static unsigned waitingFront;
static unsigned waitingBack;
static unsigned waitingNum;
static int waitingNoreply;
static int gotEvents;

static int enqueueWaiting(unsigned sequence, int replytype, void *ctx,
	X11ReplyHandler handler, const char *sarg, unsigned uarg)
{
    if (waitingNum == MAXWAITING) return -1;
    waitingReplies[waitingBack].handler = handler;
    waitingReplies[waitingBack].ctx = ctx;
    waitingReplies[waitingBack].sequence = sequence;
    waitingReplies[waitingBack].replytype = replytype;
    waitingReplies[waitingBack].sarg = sarg;
    waitingReplies[waitingBack].uarg = uarg;
    if (++waitingBack == MAXWAITING) waitingBack = 0;
    ++waitingNum;
    if (replytype == RQ_AWAIT_NOREPLY) waitingNoreply = 1;
    else if (replytype == RQ_AWAIT_REPLY) waitingNoreply = 0;
    return 0;
}

static void removeFirstWaiting(void)
{
    if (!waitingNum) return;
    --waitingNum;
    if (++waitingFront == MAXWAITING) waitingFront = 0;
}

static void handleX11Event(xcb_generic_event_t *ev)
{
    if (ev->response_type == 0)
    {
	PSC_Log_fmt(PSC_L_WARNING, "Unhandled X11 error %d: %u",
		((xcb_generic_error_t *)ev)->error_code,
		ev->sequence);
    }
    else switch (ev->response_type & 0x7f)
    {
	case XCB_CLIENT_MESSAGE:
	    PSC_Event_raise(clientmsg,
		    ((xcb_client_message_event_t *)ev)->window, ev);
	    break;

	case XCB_CONFIGURE_NOTIFY:
	    PSC_Event_raise(configureNotify,
		    ((xcb_configure_notify_event_t *)ev)->window, ev);
	    break;

	case XCB_EXPOSE:
	    PSC_Event_raise(expose,
		    ((xcb_expose_event_t *)ev)->window, ev);
	    break;

	case XCB_MAP_NOTIFY:
	    PSC_Event_raise(mapNotify,
		    ((xcb_map_notify_event_t *)ev)->window, ev);
	    break;

	case XCB_UNMAP_NOTIFY:
	    PSC_Event_raise(unmapNotify,
		    ((xcb_unmap_notify_event_t *)ev)->window, ev);
	    break;

	default:
	    PSC_Log_fmt(PSC_L_DEBUG, "Unhandled event type %hhu: %u",
		    ev->response_type & 0x7fU, ev->sequence);
	    break;
    }
    free(ev);
}

static void handleRequestError(X11ReplyHandlerRecord *rec,
	xcb_generic_error_t *err)
{
    switch (rec->replytype)
    {
	case RQ_CHECK_ERROR_STRING:
	    PSC_Log_fmt(PSC_L_ERROR, rec->ctx, rec->sarg);
	    break;

	case RQ_CHECK_ERROR_UNSIGNED:
	    PSC_Log_fmt(PSC_L_ERROR, rec->ctx, rec->uarg);
	    PSC_Event_raise(requestError, rec->uarg, 0);
	    break;

	default:
	    rec->handler(rec->ctx, rec->sequence, 0, err);
	    break;
    }
}

static void readX11Input(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    int polled = !!waitingNum;
    while (waitingNum)
    {
	void *reply = 0;
	xcb_generic_error_t *error = 0;
	if (xcb_poll_for_reply(c,
		    waitingReplies[waitingFront].sequence,
		    &reply, &error))
	{
	    if (reply)
	    {
		if (waitingReplies[waitingFront].replytype != RQ_AWAIT_REPLY)
		{
		    PSC_Log_fmt(PSC_L_ERROR, "Received unexpected reply: %u",
			    waitingReplies[waitingFront].sequence);
		    free(reply);
		    reply = 0;
		}
		else
		{
		    PSC_Log_fmt(PSC_L_DEBUG, "Received reply: %u",
			    waitingReplies[waitingFront].sequence);
		}
	    }
	    else if (error)
	    {
		PSC_Log_fmt(PSC_L_DEBUG, "Received error: %u",
			waitingReplies[waitingFront].sequence);
	    }
	    else if (waitingReplies[waitingFront].replytype
		    == RQ_AWAIT_NOREPLY)
	    {
		PSC_Log_fmt(PSC_L_DEBUG, "Confirmed request: %u",
			waitingReplies[waitingFront].sequence);
	    }
	    if (reply || error ||
		    waitingReplies[waitingFront].replytype == RQ_AWAIT_NOREPLY)
	    {
		X11ReplyHandlerRecord *rec = waitingReplies + waitingFront;
		if (error && !reply)
		{
		    handleRequestError(rec, error);
		}
		else
		{
		    rec->handler(rec->ctx, rec->sequence, reply, error);
		}
	    }
	    free(reply);
	    free(error);
	    removeFirstWaiting();
	}
	else
	{
	    xcb_generic_event_t *ev = xcb_poll_for_queued_event(c);
	    if (ev) gotEvents = 1;
	    else break;
	    while (ev && waitingReplies[waitingFront].sequence
		    - ev->sequence > 0)
	    {
		handleX11Event(ev);
		ev = xcb_poll_for_queued_event(c);
	    }
	    if (ev && ev->response_type == 0 &&
		    ev->sequence == waitingReplies[waitingFront].sequence)
	    {
		PSC_Log_fmt(PSC_L_DEBUG, "Received error: %u", ev->sequence);
		handleRequestError(waitingReplies + waitingFront,
			(xcb_generic_error_t *)ev);
		free(ev);
		removeFirstWaiting();
	    }
	    else if (ev) handleX11Event(ev);
	}
    }
    if (!waitingNum)
    {
	xcb_generic_event_t *ev = polled ?
	    xcb_poll_for_queued_event(c) : xcb_poll_for_event(c);
	if (ev) gotEvents = 1;
	while (ev)
	{
	    handleX11Event(ev);
	    ev = xcb_poll_for_queued_event(c);
	}
    }
}

static void sync_cb(void *ctx, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)ctx;
    (void)sequence;
    (void)error;

    if (!reply)
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR, "X11 sync failed");
	PSC_Service_quit();
    }
    PSC_Log_msg(PSC_L_DEBUG, "X11 connection synced");
}

static void flushandsync(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    if (gotEvents)
    {
	gotEvents = 0;
	PSC_Event_raise(eventsDone, 0, 0);
    }
    if (waitingNum)
    {
	if (waitingNoreply) AWAIT(xcb_get_input_focus(c), 0, sync_cb);
	xcb_flush(c);
    }
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
	PSC_Log_fmt(PSC_L_INFO, "using XRender version %"PRIu32".%"PRIu32,
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
    maxRequestSize = xcb_get_maximum_request_length(c) << 2;
    PSC_Log_fmt(PSC_L_DEBUG, "maximum request size is %zu bytes",
	    maxRequestSize);
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
		PSC_Log_fmt(PSC_L_INFO,
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
	if (fi.data->direct.alpha_mask != 255) continue;
	if (fi.data->depth == 8)
	{
	    if (fi.data->direct.red_mask != 0) continue;
	    if (fi.data->direct.green_mask != 0) continue;
	    if (fi.data->direct.blue_mask != 0) continue;
	    alphaformat = fi.data->id;
	}
	else if (fi.data->depth == 32)
	{
	    if (fi.data->direct.red_shift != 16) continue;
	    if (fi.data->direct.red_mask != 255) continue;
	    if (fi.data->direct.green_shift != 8) continue;
	    if (fi.data->direct.green_mask != 255) continue;
	    if (fi.data->direct.blue_shift != 0) continue;
	    if (fi.data->direct.blue_mask != 255) continue;
	    argbformat = fi.data->id;
	}
    }
    if (!alphaformat)
    {
	PSC_Log_msg(PSC_L_ERROR, "No 8bit alpha format found");
	goto error;
    }
    if (!argbformat)
    {
	PSC_Log_msg(PSC_L_ERROR, "No 32bit ARGB format found");
	goto error;
    }
    free(pf);
    pf = 0;

    clientmsg = PSC_Event_create(0);
    configureNotify = PSC_Event_create(0);
    expose = PSC_Event_create(0);
    mapNotify = PSC_Event_create(0);
    unmapNotify = PSC_Event_create(0);
    requestError = PSC_Event_create(0);
    eventsDone = PSC_Event_create(0);
    fd = xcb_get_file_descriptor(c);
    PSC_Event_register(PSC_Service_readyRead(), 0, readX11Input, fd);
    PSC_Event_register(PSC_Service_eventsDone(), 0, flushandsync, 0);
    PSC_Service_registerRead(fd);

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
    PSC_Log_fmt(PSC_L_INFO,
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

size_t X11Adapter_maxRequestSize(void)
{
    return maxRequestSize;
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

xcb_render_pictformat_t X11Adapter_argbformat(void)
{
    return argbformat;
}

PSC_Event *X11Adapter_clientmsg(void)
{
    return clientmsg;
}

PSC_Event *X11Adapter_configureNotify(void)
{
    return configureNotify;
}

PSC_Event *X11Adapter_expose(void)
{
    return expose;
}

PSC_Event *X11Adapter_mapNotify(void)
{
    return mapNotify;
}

PSC_Event *X11Adapter_unmapNotify(void)
{
    return unmapNotify;
}

PSC_Event *X11Adapter_requestError(void)
{
    return requestError;
}

PSC_Event *X11Adapter_eventsDone(void)
{
    return eventsDone;
}

const char *X11Adapter_wmClass(size_t *sz)
{
    if (sz) *sz = wmclasssz;
    return wmclass;
}

static unsigned await(unsigned sequence, int replytype, void *ctx,
	X11ReplyHandler handler, const char *sarg, unsigned uarg)
{
    if (enqueueWaiting(sequence, replytype, ctx, handler, sarg, uarg) < 0)
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR, "Reply queue is full");
	PSC_Service_quit();
    }
    switch (replytype)
    {
	case RQ_AWAIT_REPLY:
	    PSC_Log_fmt(PSC_L_DEBUG, "Awaiting Reply: %u", sequence);
	    break;

	case RQ_AWAIT_NOREPLY:
	    PSC_Log_fmt(PSC_L_DEBUG, "Awaiting Request: %u", sequence);
	    break;

	default:
	    break;
    }
    return sequence;
}

unsigned X11Adapter_await(unsigned sequence, void *ctx,
	X11ReplyHandler handler)
{
    return await(sequence, RQ_AWAIT_REPLY, ctx, handler, 0, 0);
}

unsigned X11Adapter_awaitNoreply(unsigned sequence, void *ctx,
	X11ReplyHandler handler)
{
    return await(sequence, RQ_AWAIT_NOREPLY, ctx, handler, 0, 0);
}

unsigned X11Adapter_check(unsigned sequence, void *ctx,
	X11ReplyHandler handler)
{
    return await(sequence, RQ_CHECK_ERROR, ctx, handler, 0, 0);
}

unsigned X11Adapter_checkLogUnsigned(unsigned sequence, const char *msg,
	unsigned arg)
{
    return await(sequence, RQ_CHECK_ERROR_UNSIGNED, (void *)msg, 0, 0, arg);
}

unsigned X11Adapter_checkLogString(unsigned sequence, const char *msg,
	const char *arg)
{
    return await(sequence, RQ_CHECK_ERROR_STRING, (void *)msg, 0, arg, 0);
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
    *out = 0;

    return latin1;
}

void X11Adapter_done(void)
{
    if (!c) return;
    PSC_Service_unregisterRead(fd);
    PSC_Event_unregister(PSC_Service_eventsDone(), 0, flushandsync, 0);
    PSC_Event_unregister(PSC_Service_readyRead(), 0, readX11Input, fd);
    fd = 0;
    waitingFront = 0;
    waitingBack = 0;
    waitingNum = 0;
    PSC_Event_destroy(eventsDone);
    PSC_Event_destroy(requestError);
    PSC_Event_destroy(expose);
    PSC_Event_destroy(unmapNotify);
    PSC_Event_destroy(mapNotify);
    PSC_Event_destroy(configureNotify);
    PSC_Event_destroy(clientmsg);
    eventsDone = 0;
    requestError = 0;
    expose = 0;
    unmapNotify = 0;
    mapNotify = 0;
    configureNotify = 0;
    clientmsg = 0;
    rootformat = 0;
    alphaformat = 0;
    argbformat = 0;
    xcb_disconnect(c);
    maxRequestSize = 0;
    s = 0;
    c = 0;
    memset(atoms, 0, sizeof atoms);
}


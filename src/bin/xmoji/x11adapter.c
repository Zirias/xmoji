#include "x11adapter.h"

#include "suppress.h"
#include "unistr.h"

#include <inttypes.h>
#include <locale.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcbext.h>
SUPPRESS(pedantic)
#include <xcb/xkb.h>
ENDSUPPRESS
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#define MAXWAITING 128
#define SYNCTHRESH 112

#define RQ_AWAIT_REPLY 1
#define RQ_AWAIT_NOREPLY 0
#define RQ_CHECK_ERROR -1
#define RQ_CHECK_ERROR_UNSIGNED -2
#define RQ_CHECK_ERROR_STRING -3

typedef struct X11ReplyHandlerRecord
{
    X11ReplyHandler handler;
    void *ctx;
    const char *sarg;
    xcb_generic_error_t *err;
    unsigned sequence;
    int replytype;
    unsigned uarg;
} X11ReplyHandlerRecord;

typedef struct XkbEvent
{
    uint8_t response_type;
    uint8_t xkbType;
    uint16_t sequence;
    xcb_timestamp_t time;
    uint8_t deviceId;
} XkbEvent;

static const struct { int len; const char *nm; } atomnm[] = {
    X_ATOMS(X_SZNM)
};

static const char *modnm[] = {
    XKB_MOD_NAME_LOGO,
    XKB_MOD_NAME_NUM,
    XKB_MOD_NAME_ALT,
    XKB_MOD_NAME_CTRL,
    XKB_MOD_NAME_CAPS,
    XKB_MOD_NAME_SHIFT
};
#define NUMMODS (sizeof modnm / sizeof *modnm)

#define XKBEVENTS ( \
	XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | \
	XCB_XKB_EVENT_TYPE_MAP_NOTIFY | \
	XCB_XKB_EVENT_TYPE_STATE_NOTIFY )
#define XKBMAPPARTS ( \
	XCB_XKB_MAP_PART_KEY_TYPES | \
	XCB_XKB_MAP_PART_KEY_SYMS | \
	XCB_XKB_MAP_PART_MODIFIER_MAP | \
	XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS | \
	XCB_XKB_MAP_PART_KEY_ACTIONS | \
	XCB_XKB_MAP_PART_VIRTUAL_MODS | \
	XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP )
#define XKBKBDETAILS ( \
	XCB_XKB_NKN_DETAIL_KEYCODES )
#define XKBSTDETAILS ( \
	XCB_XKB_STATE_PART_MODIFIER_BASE | \
	XCB_XKB_STATE_PART_MODIFIER_LATCH | \
	XCB_XKB_STATE_PART_MODIFIER_LOCK | \
	XCB_XKB_STATE_PART_GROUP_BASE | \
	XCB_XKB_STATE_PART_GROUP_LATCH | \
	XCB_XKB_STATE_PART_GROUP_LOCK )
static const xcb_xkb_select_events_details_t xkbevdetails = {
    .affectNewKeyboard = XKBKBDETAILS,
    .newKeyboardDetails = XKBKBDETAILS,
    .affectState = XKBSTDETAILS,
    .stateDetails = XKBSTDETAILS
};

static char wmclass[512];
static size_t wmclasssz;

static xcb_connection_t *c;
static xcb_screen_t *s;
static struct xkb_context *kbdctx;
static struct xkb_keymap *keymap;
static struct xkb_compose_table *kbdcompose;
static struct xkb_state *kbdstate;
static size_t maxRequestSize;
static PSC_Event *clientmsg;
static PSC_Event *configureNotify;
static PSC_Event *expose;
static PSC_Event *keypress;
static PSC_Event *mapNotify;
static PSC_Event *unmapNotify;
static PSC_Event *requestError;
static PSC_Event *eventsDone;
static xcb_atom_t atoms[NATOMS];
static xcb_render_pictformat_t rootformat;
static xcb_render_pictformat_t alphaformat;
static xcb_render_pictformat_t argbformat;
static int32_t kbdid;
static int fd;
static uint8_t xkbevbase;
static int modmap[NUMMODS];

static X11ReplyHandlerRecord waitingReplies[MAXWAITING];
static unsigned waitingFront;
static unsigned waitingBack;
static unsigned waitingNum;
static unsigned syncseq;
static int waitingNoreply;

static int enqueueWaiting(unsigned sequence, int replytype, void *ctx,
	X11ReplyHandler handler, const char *sarg, unsigned uarg)
{
    if (waitingNum == MAXWAITING) return -1;
    waitingReplies[waitingBack].handler = handler;
    waitingReplies[waitingBack].ctx = ctx;
    waitingReplies[waitingBack].sarg = sarg;
    waitingReplies[waitingBack].err = 0;
    waitingReplies[waitingBack].sequence = sequence;
    waitingReplies[waitingBack].replytype = replytype;
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

static X11ReplyHandlerRecord *findWaitingBySequence(unsigned sequence)
{
    if (!waitingNum) return 0;
    unsigned i = waitingFront;
    do
    {
	if (waitingReplies[i].sequence == sequence) return waitingReplies + i;
	if ((int)(waitingReplies[i].sequence - sequence) > 0) return 0;
	if (++i == MAXWAITING) i = 0;
    } while (i != waitingBack);
    return 0;
}

static void updateKeymap(void)
{
    struct xkb_keymap *newKeymap = xkb_x11_keymap_new_from_device(kbdctx,
	    c, kbdid, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!newKeymap)
    {
	PSC_Log_msg(PSC_L_WARNING, "Could not update keymap");
	return;
    }
    struct xkb_state *newKbdstate = xkb_x11_state_new_from_device(newKeymap,
	    c, kbdid);
    if (!newKbdstate)
    {
	PSC_Log_msg(PSC_L_WARNING,
		"Could not initialize state for new kexymap");
	xkb_keymap_unref(newKeymap);
	return;
    }
    xkb_state_unref(kbdstate);
    xkb_keymap_unref(keymap);
    keymap = newKeymap;
    kbdstate = newKbdstate;
    for (unsigned i = 0; i < NUMMODS; ++i)
    {
	modmap[i] = xkb_keymap_mod_get_index(keymap, modnm[i]);
    }
}

static void handleXkbEvent(XkbEvent *ev)
{
    xcb_xkb_state_notify_event_t *snev;

    if (ev->deviceId != kbdid) return;

    switch (ev->xkbType)
    {
	case XCB_XKB_NEW_KEYBOARD_NOTIFY:
	    if (((xcb_xkb_new_keyboard_notify_event_t *)ev)->changed
		    & XCB_XKB_NKN_DETAIL_KEYCODES) updateKeymap();
	    break;

	case XCB_XKB_MAP_NOTIFY:
	    updateKeymap();
	    break;

	case XCB_XKB_STATE_NOTIFY:
	    snev = (xcb_xkb_state_notify_event_t *)ev;
	    xkb_state_update_mask(kbdstate, snev->baseMods, snev->latchedMods,
		    snev->lockedMods, snev->baseGroup, snev->latchedGroup,
		    snev->lockedGroup);
	    break;

	default:
	    break;
    }
}

static void handleX11Reply(X11ReplyHandlerRecord *rec, void *reply,
	xcb_generic_error_t *error)
{
    if (reply)
    {
	if (rec->replytype != RQ_AWAIT_REPLY)
	{
	    PSC_Log_fmt(PSC_L_ERROR, "Received unexpected reply: %u",
		    rec->sequence);
	    free(reply);
	    reply = 0;
	}
	else
	{
	    PSC_Log_fmt(PSC_L_DEBUG, "Received reply: %u", rec->sequence);
	    rec->handler(rec->ctx, rec->sequence, reply, error);
	}
    }
    else if (error)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Received error: %u", rec->sequence);
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
		rec->handler(rec->ctx, rec->sequence, reply, error);
		break;
	}
    }
    else if (rec->replytype == RQ_AWAIT_NOREPLY)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Confirmed request: %u", rec->sequence);
	rec->handler(rec->ctx, rec->sequence, reply, error);
    }
    free(reply);
    free(error);
    removeFirstWaiting();
}

static void handleX11Event(xcb_generic_event_t *ev)
{
    if (ev->response_type == 0)
    {
	// Try to match the error to an outstanding request
	X11ReplyHandlerRecord *rec = findWaitingBySequence(ev->sequence);
	if (rec)
	{
	    if (rec->err)
	    {
		PSC_Log_fmt(PSC_L_ERROR, "Received second error for %u",
			ev->sequence);
		free(rec->err);
	    }
	    rec->err = (xcb_generic_error_t *)ev;
	    return;
	}
	else
	{
	    // Received an error that couldn't be matched to a request
	    PSC_Log_fmt(PSC_L_WARNING, "Unhandled X11 error %d: %u",
		    ((xcb_generic_error_t *)ev)->error_code,
		    ev->sequence);
	}
    }

    else if (ev->response_type == xkbevbase)
    {
	handleXkbEvent((XkbEvent *)ev);
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

	case XCB_KEY_PRESS:
	    {
		xcb_key_press_event_t *kpev = (xcb_key_press_event_t *)ev;
		xkb_keycode_t key = kpev->detail;
		XkbModifier mods = XM_NONE;
		XkbModifier rawmods = XM_NONE;
		for (unsigned i = 0; i < NUMMODS; ++i)
		{
		    mods <<= 1;
		    rawmods <<= 1;
		    if (modmap[i] >= 0 && xkb_state_mod_index_is_active(
				kbdstate, modmap[i], XKB_STATE_MODS_EFFECTIVE))
		    {
			if (xkb_state_mod_index_is_consumed2(kbdstate, key,
				    modmap[i], XKB_CONSUMED_MODE_GTK) == 0)
			{
			    mods |= 1;
			}
			rawmods |= 1;
		    }
		}
		XkbKeyEventArgs ea = {
		    .keycode = key,
		    .keysym = xkb_state_key_get_one_sym(kbdstate, key),
		    .modifiers = mods,
		    .rawmods = rawmods
		};
		PSC_Event_raise(keypress, kpev->event, &ea);
	    }
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

static void readX11Input(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    /* This function is called from the main event loop when the X11 socket is
     * ready to read. We will block on select() before this is called again,
     * therefore we must make sure to always process all input already read and
     * queued by xcb, otherwise X11 events could sit unnoticed until new input
     * from the X server arrives.
     */

    int gotinput = 1;
    while (gotinput)
    {
	gotinput = 0; // whether we found new input in this iteration

	/* Initialize a flag whether we attempted to read new input in this
	 * loop iteration. When waiting for the result of a request, we must
	 * call xcb_poll_for_reply() to check for it. This function might
	 * always attempt to read new input (and buffer X11 events in xcb's
	 * queue).
	 */
	int polled = !!waitingNum;

	while (waitingNum)
	{
	    void *reply = 0;
	    xcb_generic_error_t *error = 0;
	    X11ReplyHandlerRecord *rec = waitingReplies + waitingFront;

	    gotinput = xcb_poll_for_reply(c, rec->sequence, &reply, &error);
	    if (gotinput)
	    {
		if (!reply && !error && !rec->err)
		{
		    /* The request is considered completed, but we got neither
		     * a reply nor an error, then process queued events first
		     * to collect errors potentially delivered there.
		     */
		    xcb_generic_event_t *ev;
		    while ((ev = xcb_poll_for_queued_event(c)))
		    {
			handleX11Event(ev);
		    }
		}
		if (rec->err)
		{
		    if (error)
		    {
			PSC_Log_fmt(PSC_L_ERROR,
				"Received second error for %u", rec->sequence);
			free(rec->err);
		    }
		    else error = rec->err;
		}
		/* Handle reply or error to the first request we're waiting
		 * for, according to what was registered by AWAIT() or CHECK().
		 */
		handleX11Reply(rec, reply, error);
	    }
	    else
	    {
		/* If we didn't find a completed request, switch to
		 * processing events.
		 */
		break;
	    }
	}

	xcb_generic_event_t *ev;

	/* If we already attempted to read new input in this iteration,
	 * just process what's already queued.
	 */
	if (polled) ev = xcb_poll_for_queued_event(c);

	/* Otherwise, attempt once to read new input and update our flag
	 * whether new input was available.
	 */
	else if ((ev = xcb_poll_for_event(c))) gotinput = 1;

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
    syncseq = 0;
    PSC_Log_msg(PSC_L_DEBUG, "X11 connection synced");
}

static void flushandsync(void *receiver, void *sender, void *args)
{
    (void)receiver;
    (void)sender;
    (void)args;

    PSC_Event_raise(eventsDone, 0, 0);
    if (waitingNum)
    {
	if (!syncseq && (waitingNoreply || waitingNum >= SYNCTHRESH))
	{
	    syncseq = AWAIT(xcb_get_input_focus(c), 0, sync_cb);
	}
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
    free(pf);
    pf = 0;
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

    uint16_t xkbmaj;
    uint16_t xkbmin;
    if (xkb_x11_setup_xkb_extension(c, XKB_X11_MIN_MAJOR_XKB_VERSION,
		XKB_X11_MIN_MINOR_XKB_VERSION,
		XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, &xkbmaj, &xkbmin,
		&xkbevbase, 0))
    {
	PSC_Log_fmt(PSC_L_INFO, "using XKB version %"PRIu16".%"PRIu16,
		xkbmaj, xkbmin);
    }
    else goto error;
    kbdid = xkb_x11_get_core_keyboard_device_id(c);
    if (kbdid == -1)
    {
	PSC_Log_msg(PSC_L_ERROR, "No core keyboard found");
	goto error;
    }
    xcb_void_cookie_t xkbeventscookie = xcb_xkb_select_events_aux_checked(c,
	    kbdid, XKBEVENTS, 0, 0, XKBMAPPARTS, XKBMAPPARTS, &xkbevdetails);
    kbdctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!kbdctx)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not create XKB context");
	goto error;
    }

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
	PSC_Log_msg(PSC_L_ERROR, "Couldn't set locale");
	goto error;
    }
    kbdcompose = xkb_compose_table_new_from_locale(kbdctx, lc,
	    XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!kbdcompose)
    {
	PSC_Log_fmt(PSC_L_ERROR, "Couldn't create compose table for `%s'", lc);
	goto error;
    }
    xcb_generic_error_t *rqerr = xcb_request_check(c, xkbeventscookie);
    if (rqerr)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not request XKB events");
	free(rqerr);
	goto error;
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
    nm = LATIN1(nm);
    char *clnm = 0;
    if (classname)
    {
	clnm = LATIN1(classname);
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

    clientmsg = PSC_Event_create(0);
    configureNotify = PSC_Event_create(0);
    expose = PSC_Event_create(0);
    keypress = PSC_Event_create(0);
    mapNotify = PSC_Event_create(0);
    unmapNotify = PSC_Event_create(0);
    requestError = PSC_Event_create(0);
    eventsDone = PSC_Event_create(0);
    fd = xcb_get_file_descriptor(c);
    PSC_Event_register(PSC_Service_readyRead(), 0, readX11Input, fd);
    PSC_Event_register(PSC_Service_eventsDone(), 0, flushandsync, 0);
    PSC_Service_registerRead(fd);

    updateKeymap();

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

struct xkb_compose_table *X11Adapter_kbdcompose(void)
{
    return kbdcompose;
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

PSC_Event *X11Adapter_keypress(void)
{
    return keypress;
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
    PSC_Event_destroy(unmapNotify);
    PSC_Event_destroy(mapNotify);
    PSC_Event_destroy(keypress);
    PSC_Event_destroy(expose);
    PSC_Event_destroy(configureNotify);
    PSC_Event_destroy(clientmsg);
    eventsDone = 0;
    requestError = 0;
    unmapNotify = 0;
    mapNotify = 0;
    keypress = 0;
    expose = 0;
    configureNotify = 0;
    clientmsg = 0;
    rootformat = 0;
    alphaformat = 0;
    argbformat = 0;
    xkb_state_unref(kbdstate);
    kbdstate = 0;
    xkb_keymap_unref(keymap);
    keymap = 0;
    xkb_compose_table_unref(kbdcompose);
    kbdcompose = 0;
    xkb_context_unref(kbdctx);
    kbdctx = 0;
    xcb_disconnect(c);
    maxRequestSize = 0;
    s = 0;
    c = 0;
    memset(atoms, 0, sizeof atoms);
}


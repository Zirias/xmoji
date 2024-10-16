#include "x11adapter.h"

#include "font.h"
#include "suppress.h"
#include "unistr.h"
#include "xrdb.h"

#include <inttypes.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_image.h>
#include <xcb/xcbext.h>
SUPPRESS(pedantic)
#include <xcb/xkb.h>
ENDSUPPRESS
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#define MAXWAITING 16384
#define SYNCTHRESH 8192
#define MAXCOLORS 64

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
#ifdef TRACE_X11_REQUESTS
    const char *reqsource;
    const char *sourcefile;
    const char *function;
    unsigned lineno;
#endif
    unsigned sequence;
    int replytype;
    unsigned uarg;
} X11ReplyHandlerRecord;

C_CLASS_DECL(ColorMapCallback);
struct ColorMapCallback
{
    ColorMapCallback *next;
    void *ctx;
    MapColorHandler handler;
};

typedef struct ColorMapEntry
{
    Color color;
    uint32_t pixel;
    int ref;
    ColorMapCallback *callbacks;
} ColorMapEntry;

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

static const char *cursornames[] = {
    "left_ptr",
    "xterm",
    "hand2",
    "watch"
};
static xcb_cursor_t cursors[sizeof cursornames / sizeof *cursornames];

static char wmclass[512];
static size_t wmclasssz;
static double dpi = 96.;

static xcb_connection_t *c;
static xcb_screen_t *s;
uint64_t rdpos;
static XGlitch glitches;
static XRdb *rdb;
static struct xkb_context *kbdctx;
static struct xkb_keymap *keymap;
static struct xkb_compose_table *kbdcompose;
static struct xkb_state *kbdstate;
static xcb_cursor_context_t *cctx;
static size_t maxRequestSize;
static PSC_Event *buttonpress;
static PSC_Event *buttonrelease;
static PSC_Event *clientmsg;
static PSC_Event *configureNotify;
static PSC_Event *enter;
static PSC_Event *expose;
static PSC_Event *focusin;
static PSC_Event *focusout;
static PSC_Event *keypress;
static PSC_Event *leave;
static PSC_Event *mapNotify;
static PSC_Event *motionNotify;
static PSC_Event *propertyNotify;
static PSC_Event *selectionClear;
static PSC_Event *selectionNotify;
static PSC_Event *selectionRequest;
static PSC_Event *unmapNotify;
static PSC_Event *requestError;
static PSC_Event *eventsDone;
static xcb_atom_t atoms[NATOMS];
static xcb_render_pictformat_t rootformat;
static xcb_render_pictformat_t formats[3];
static int32_t kbdid;
static int fd;
static uint8_t xkbevbase;
static int modmap[NUMMODS];

static X11ReplyHandlerRecord waitingReplies[MAXWAITING];
static unsigned waitingFront;
static unsigned waitingBack;
static unsigned waitingNum;
static unsigned newWaiting;
static unsigned syncseq;
static int waitingNoreply;

static ColorMapEntry colorMap[MAXCOLORS];

static int enqueueWaiting(X11RequestId reqid, int replytype, void *ctx,
	X11ReplyHandler handler, const char *sarg, unsigned uarg)
{
    if (waitingNum == MAXWAITING) return -1;
    waitingReplies[waitingBack].handler = handler;
    waitingReplies[waitingBack].ctx = ctx;
    waitingReplies[waitingBack].sarg = sarg;
    waitingReplies[waitingBack].err = 0;
#ifdef TRACE_X11_REQUESTS
    waitingReplies[waitingBack].reqsource = reqid.reqsource;
    waitingReplies[waitingBack].sourcefile = reqid.sourcefile;
    waitingReplies[waitingBack].function = reqid.function;
    waitingReplies[waitingBack].lineno = reqid.lineno;
    waitingReplies[waitingBack].sequence = reqid.sequence;
#else
    waitingReplies[waitingBack].sequence = reqid;
#endif
    waitingReplies[waitingBack].replytype = replytype;
    waitingReplies[waitingBack].uarg = uarg;
    if (++waitingBack == MAXWAITING) waitingBack = 0;
    ++waitingNum;
    ++newWaiting;
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
		RequestErrorEventArgs ea = {
#ifdef TRACE_X11_REQUESTS
		    .reqid = {
			.reqsource = rec->reqsource,
			.sourcefile = rec->sourcefile,
			.function = rec->function,
			.lineno = rec->lineno,
			.sequence = rec->sequence
		    },
#else
		    .reqid = rec->sequence,
#endif
		    .opMinor = error->minor_code,
		    .opMajor = error->major_code,
		    .code = error->error_code
		};
		PSC_Event_raise(requestError, rec->uarg, &ea);
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
	case XCB_BUTTON_PRESS:
	    PSC_Event_raise(buttonpress,
		    ((xcb_button_press_event_t *)ev)->event, ev);
	    break;

	case XCB_BUTTON_RELEASE:
	    PSC_Event_raise(buttonrelease,
		    ((xcb_button_release_event_t *)ev)->event, ev);
	    break;

	case XCB_CLIENT_MESSAGE:
	    PSC_Event_raise(clientmsg,
		    ((xcb_client_message_event_t *)ev)->window, ev);
	    break;

	case XCB_CONFIGURE_NOTIFY:
	    PSC_Event_raise(configureNotify,
		    ((xcb_configure_notify_event_t *)ev)->window, ev);
	    break;

	case XCB_ENTER_NOTIFY:
	    PSC_Event_raise(enter,
		    ((xcb_enter_notify_event_t *)ev)->event, ev);
	    break;

	case XCB_EXPOSE:
	    PSC_Event_raise(expose,
		    ((xcb_expose_event_t *)ev)->window, ev);
	    break;

	case XCB_FOCUS_IN:
	    PSC_Event_raise(focusin,
		    ((xcb_focus_in_event_t *)ev)->event, ev);
	    break;

	case XCB_FOCUS_OUT:
	    PSC_Event_raise(focusout,
		    ((xcb_focus_out_event_t *)ev)->event, ev);
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

	case XCB_LEAVE_NOTIFY:
	    PSC_Event_raise(leave,
		    ((xcb_leave_notify_event_t *)ev)->event, ev);
	    break;

	case XCB_MAP_NOTIFY:
	    PSC_Event_raise(mapNotify,
		    ((xcb_map_notify_event_t *)ev)->window, ev);
	    break;

	case XCB_MOTION_NOTIFY:
	    PSC_Event_raise(motionNotify,
		    ((xcb_motion_notify_event_t *)ev)->event, ev);
	    break;

	case XCB_PROPERTY_NOTIFY:
	    PSC_Event_raise(propertyNotify,
		    ((xcb_property_notify_event_t *)ev)->window, ev);
	    break;

	case XCB_SELECTION_CLEAR:
	    PSC_Event_raise(selectionClear,
		    ((xcb_selection_clear_event_t *)ev)->owner, ev);
	    break;

	case XCB_SELECTION_NOTIFY:
	    PSC_Event_raise(selectionNotify,
		    ((xcb_selection_notify_event_t *)ev)->requestor, ev);
	    break;

	case XCB_SELECTION_REQUEST:
	    PSC_Event_raise(selectionRequest,
		    ((xcb_selection_request_event_t *)ev)->owner, ev);
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
     * ready to read, or at the end of the event loop when xcb read more bytes
     * meanwhile. We will then block on select() before this is called again,
     * therefore we must make sure to always process all input already read and
     * queued by xcb, otherwise X11 events could sit unnoticed until new input
     * from the X server arrives.
     */

    /* Determine how many bytes xcb read before starting to process */
    uint64_t newrdpos = xcb_total_read(c);
    do
    {
	rdpos = newrdpos;
	while (waitingNum)
	{
	    void *reply = 0;
	    xcb_generic_error_t *error = 0;
	    X11ReplyHandlerRecord *rec = waitingReplies + waitingFront;

	    if (xcb_poll_for_reply(c, rec->sequence, &reply, &error))
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

	xcb_generic_event_t *ev = xcb_poll_for_event(c);
	while (ev)
	{
	    handleX11Event(ev);
	    ev = xcb_poll_for_queued_event(c);
	}

	/* Check whether we read new data */
	newrdpos = xcb_total_read(c);

    } while (newrdpos != rdpos); /* repeat as long as new input was read */
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

    /* Trigger event for end of event-loop iteration */
    PSC_Event_raise(eventsDone, 0, 0);

    do
    {
	/* Repeat this reading new input until it did not trigger reading
	 * more data */
	while (xcb_total_read(c) != rdpos)
	{
	    readX11Input(0, 0, 0);
	    PSC_Event_raise(eventsDone, 0, 0);
	}

	/* Finally check whether a sync is needed */
	if (!syncseq && (waitingNoreply || waitingNum >= SYNCTHRESH))
	{
	    syncseq = AWAIT(xcb_get_input_focus(c), 0, sync_cb);
	    xcb_flush(c);
	}
	else if (newWaiting)
	{
	    xcb_flush(c);
	    newWaiting = 0;
	}

	/* Check again whether xcb_flush() could have triggered a read,
	 * repeat the whole dance in this case */
    } while (xcb_total_read(c) != rdpos);
}

int X11Adapter_init(int argc, char **argv, const char *locale,
	const char *name, const char *classname)
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
    dpi = (((double) s->height_in_pixels) * 25.4) /
	((double) s->height_in_millimeters);

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
	if (fi.data->depth == 8)
	{
	    if (fi.data->direct.red_mask != 0) continue;
	    if (fi.data->direct.green_mask != 0) continue;
	    if (fi.data->direct.blue_mask != 0) continue;
	    if (fi.data->direct.alpha_mask != 255) continue;
	    formats[PICTFORMAT_ALPHA] = fi.data->id;
	}
	else
	{
	    if (fi.data->direct.red_shift != 16) continue;
	    if (fi.data->direct.red_mask != 255) continue;
	    if (fi.data->direct.green_shift != 8) continue;
	    if (fi.data->direct.green_mask != 255) continue;
	    if (fi.data->direct.blue_shift != 0) continue;
	    if (fi.data->direct.blue_mask != 255) continue;
	    if (fi.data->depth == 24 && fi.data->direct.alpha_mask == 0)
	    {
		formats[PICTFORMAT_RGB] = fi.data->id;
	    }
	    else if (fi.data->depth == 32 && fi.data->direct.alpha_mask == 255)
	    {
		formats[PICTFORMAT_ARGB] = fi.data->id;
	    }
	}
    }
    free(pf);
    pf = 0;
    if (!formats[PICTFORMAT_ALPHA])
    {
	PSC_Log_msg(PSC_L_ERROR, "No 8bit alpha format found");
	goto error;
    }
    if (!formats[PICTFORMAT_RGB])
    {
	PSC_Log_msg(PSC_L_ERROR, "No 24bit RGB format found");
	goto error;
    }
    if (!formats[PICTFORMAT_ARGB])
    {
	PSC_Log_msg(PSC_L_ERROR, "No 32bit ARGB format found");
	goto error;
    }

    glitches = 0;
    xcb_render_glyphset_t probeset = xcb_generate_id(c);
    xcb_render_create_glyph_set(c, probeset, formats[PICTFORMAT_ALPHA]);
    xcb_render_glyphinfo_t probeglyph = { 1, 1, 0, 0, 0, 0 };
    uint8_t probebitmap[] = { 0xff, 0xff, 0xff, 0xff };
    uint32_t probegid = 0;
    xcb_render_add_glyphs(c, probeset, 1, &probegid, &probeglyph,
	    4, probebitmap);
    xcb_pixmap_t probepixmap = xcb_generate_id(c);
    xcb_create_pixmap(c, 8, probepixmap, s->root, 1, 1);
    xcb_render_picture_t probesrc = xcb_generate_id(c);
    xcb_render_create_picture(c, probesrc, probepixmap,
	    formats[PICTFORMAT_ALPHA], 0, 0);
    xcb_free_pixmap(c, probepixmap);
    xcb_rectangle_t proberect = { 0, 0, 1, 1 };
    Color probecolor = 0xffffffff;
    xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC, probesrc,
	    Color_xcb(probecolor), 1, &proberect);
    probepixmap = xcb_generate_id(c);
    xcb_create_pixmap(c, 8, probepixmap, s->root, 1, 1);
    xcb_render_picture_t probedst = xcb_generate_id(c);
    xcb_render_create_picture(c, probedst, probepixmap,
	    formats[PICTFORMAT_ALPHA], 0, 0);
    probecolor = 0;
    xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_SRC, probedst,
	    Color_xcb(probecolor), 1, &proberect);
    GlyphRenderInfo proberender =  { 1, { 0, 0, 0 }, 0, 0, 0 };
    xcb_render_composite_glyphs_32(c, XCB_RENDER_PICT_OP_OVER, probesrc,
	    probedst, 0, probeset, 0, 1, sizeof proberender,
	    (const uint8_t *)&proberender);
    xcb_render_free_picture(c, probedst);
    xcb_render_free_picture(c, probesrc);
    xcb_render_free_glyph_set(c, probeset);
    xcb_image_t *probeimg = xcb_image_get(c, probepixmap, 0, 0, 1, 1,
	    0xffffffff, XCB_IMAGE_FORMAT_Z_PIXMAP);
    xcb_free_pixmap(c, probepixmap);
    if (probeimg->data[0])
    {
	PSC_Log_msg(PSC_L_INFO,
		"Detected glyph rendering glitch, enabling workaround.");
	glitches |= XG_RENDER_SRC_OFFSET;
    }
    xcb_image_destroy(probeimg);

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

    uint32_t maxproplen = (maxRequestSize
	    - sizeof(xcb_get_property_reply_t)) >> 2;
    xcb_get_property_cookie_t rescookie = xcb_get_property(c, 0, s->root,
	    XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0, maxproplen);

    kbdcompose = xkb_compose_table_new_from_locale(kbdctx, locale,
	    XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!kbdcompose)
    {
	PSC_Log_fmt(PSC_L_ERROR,
		"Couldn't create compose table for `%s'", locale);
	goto error;
    }

    char *nm = LATIN1(name);
    char *cnm = LATIN1(classname);
    int len = snprintf(wmclass, sizeof wmclass / 2, "%s", nm);
    size_t pos = len + 1;
    if (pos > sizeof wmclass / 2) pos = sizeof wmclass / 2;
    int len2 = snprintf(wmclass+pos, sizeof wmclass / 2, "%s", cnm);
    PSC_Log_fmt(PSC_L_INFO,
	    "starting with window class \"%s\", \"%s\"", nm, classname);
    free(cnm);
    free(nm);
    wmclasssz = pos + len2 + 1;
    if (wmclasssz > sizeof wmclass) wmclasssz = sizeof wmclass;
    xcb_generic_error_t *rqerr = xcb_request_check(c, xkbeventscookie);
    if (rqerr)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not request XKB events");
	free(rqerr);
	goto error;
    }
    xcb_get_property_reply_t *resreply = xcb_get_property_reply(c,
	    rescookie, &rqerr);
    if (rqerr) free(rqerr);
    if (resreply)
    {
	if (resreply->bytes_after)
	{
	    char *resstr = 0;
	    size_t ressz = 0;
	    while (resreply)
	    {
		size_t recvlen = xcb_get_property_value_length(resreply);
		ressz &= ~3;
		resstr = PSC_realloc(resstr, ressz + recvlen);
		memcpy(resstr + ressz,
			xcb_get_property_value(resreply), recvlen);
		ressz += recvlen;
		if (!resreply->bytes_after) break;
		free(resreply);
		rescookie = xcb_get_property(c, 0, s->root,
			XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING,
			ressz >> 2, maxproplen);
		resreply = xcb_get_property_reply(c, rescookie, &rqerr);
		if (rqerr) free(rqerr);
	    }
	    if (resreply)
	    {
		rdb = XRdb_create(resstr, ressz, classname, name);
		free(resreply);
	    }
	    free(resstr);
	}
	else
	{
	    rdb = XRdb_create(xcb_get_property_value(resreply),
		    xcb_get_property_value_length(resreply),
		    classname, name);
	    free(resreply);
	}
    }
    if (!rdb) rdb = XRdb_create(0, 0, classname, name);
    XRdb_setOverrides(rdb, argc, argv);
    double resdpi = XRdb_float(rdb, XRdbKey("dpi"), XRQF_OVERRIDES,
	    0., 72., 512.);
    if (resdpi) dpi = resdpi;
    PSC_Log_fmt(PSC_L_INFO, "Screen resolution%s: %.2f dpi",
	    resdpi ? " (overridden)" : "", dpi);

    if (xcb_cursor_context_new(c, s, &cctx) < 0)
    {
	PSC_Log_msg(PSC_L_ERROR, "Could not initialize XCursor");
	goto error;
    }
    for (unsigned i = 0; i < sizeof cursornames / sizeof *cursornames; ++i)
    {
	cursors[i] = xcb_cursor_load_cursor(cctx, cursornames[i]);
    }

    buttonpress = PSC_Event_create(0);
    buttonrelease = PSC_Event_create(0);
    clientmsg = PSC_Event_create(0);
    configureNotify = PSC_Event_create(0);
    enter = PSC_Event_create(0);
    expose = PSC_Event_create(0);
    focusin = PSC_Event_create(0);
    focusout = PSC_Event_create(0);
    keypress = PSC_Event_create(0);
    leave = PSC_Event_create(0);
    mapNotify = PSC_Event_create(0);
    motionNotify = PSC_Event_create(0);
    propertyNotify = PSC_Event_create(0);
    selectionClear = PSC_Event_create(0);
    selectionNotify = PSC_Event_create(0);
    selectionRequest = PSC_Event_create(0);
    unmapNotify = PSC_Event_create(0);
    requestError = PSC_Event_create(0);
    eventsDone = PSC_Event_create(0);
    fd = xcb_get_file_descriptor(c);
    PSC_Event_register(PSC_Service_readyRead(), 0, readX11Input, fd);
    PSC_Event_register(PSC_Service_eventsDone(), 0, flushandsync, 0);
    PSC_Service_registerRead(fd);

    updateKeymap();

    rdpos = xcb_total_read(c);
    return 0;

error:
    free(pf);
    X11Adapter_done();
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

XRdb *X11Adapter_resources(void)
{
    return rdb;
}

size_t X11Adapter_maxRequestSize(void)
{
    return maxRequestSize;
}

XGlitch X11Adapter_glitches(void)
{
    return glitches;
}

xcb_atom_t X11Adapter_atom(XAtomId id)
{
    return atoms[id];
}

xcb_render_pictformat_t X11Adapter_rootformat(void)
{
    return rootformat;
}

xcb_render_pictformat_t X11Adapter_format(PictFormat format)
{
    return formats[format];
}

struct xkb_compose_table *X11Adapter_kbdcompose(void)
{
    return kbdcompose;
}

PSC_Event *X11Adapter_buttonpress(void)
{
    return buttonpress;
}

PSC_Event *X11Adapter_buttonrelease(void)
{
    return buttonrelease;
}

PSC_Event *X11Adapter_clientmsg(void)
{
    return clientmsg;
}

PSC_Event *X11Adapter_configureNotify(void)
{
    return configureNotify;
}

PSC_Event *X11Adapter_enter(void)
{
    return enter;
}

PSC_Event *X11Adapter_expose(void)
{
    return expose;
}

PSC_Event *X11Adapter_focusin(void)
{
    return focusin;
}

PSC_Event *X11Adapter_focusout(void)
{
    return focusout;
}

PSC_Event *X11Adapter_keypress(void)
{
    return keypress;
}

PSC_Event *X11Adapter_leave(void)
{
    return leave;
}

PSC_Event *X11Adapter_mapNotify(void)
{
    return mapNotify;
}

PSC_Event *X11Adapter_motionNotify(void)
{
    return motionNotify;
}

PSC_Event *X11Adapter_propertyNotify(void)
{
    return propertyNotify;
}

PSC_Event *X11Adapter_selectionClear(void)
{
    return selectionClear;
}

PSC_Event *X11Adapter_selectionNotify(void)
{
    return selectionNotify;
}

PSC_Event *X11Adapter_selectionRequest(void)
{
    return selectionRequest;
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

xcb_cursor_t X11Adapter_cursor(XCursor id)
{
    if (id < 0 || (unsigned)id > sizeof cursornames / sizeof *cursornames)
    {
	return 0;
    }
    return cursors[id];
}

double X11Adapter_dpi(void)
{
    return dpi;
}

static unsigned await(X11RequestId reqid, int replytype, void *ctx,
	X11ReplyHandler handler, const char *sarg, unsigned uarg)
{
    if (enqueueWaiting(reqid, replytype, ctx, handler, sarg, uarg) < 0)
    {
	PSC_Log_setAsync(0);
	PSC_Log_msg(PSC_L_ERROR, "Reply queue is full");
	PSC_Service_quit();
    }
#ifdef TRACE_X11_REQUESTS
    return reqid.sequence;
#else
    return reqid;
#endif
}

unsigned X11Adapter_await(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler)
{
    return await(reqid, RQ_AWAIT_REPLY, ctx, handler, 0, 0);
}

unsigned X11Adapter_awaitNoreply(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler)
{
    return await(reqid, RQ_AWAIT_NOREPLY, ctx, handler, 0, 0);
}

unsigned X11Adapter_check(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler)
{
    return await(reqid, RQ_CHECK_ERROR, ctx, handler, 0, 0);
}

unsigned X11Adapter_checkLogUnsigned(X11RequestId reqid, const char *msg,
	unsigned arg)
{
    return await(reqid, RQ_CHECK_ERROR_UNSIGNED, (void *)msg, 0, 0, arg);
}

unsigned X11Adapter_checkLogString(X11RequestId reqid, const char *msg,
	const char *arg)
{
    return await(reqid, RQ_CHECK_ERROR_STRING, (void *)msg, 0, arg, 0);
}

static void receiveColor(void *ctx, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    ColorMapEntry *entry = ctx;
    if (error || !reply)
    {
	PSC_Log_msg(PSC_L_WARNING, "X11Adapter: cannot allocate color");
	entry->ref = 0;
	entry->pixel = s->black_pixel;
    }
    else
    {
	xcb_alloc_color_reply_t *color = reply;
	entry->pixel = color->pixel;
    }
    ColorMapCallback *cb = entry->callbacks;
    while (cb)
    {
	ColorMapCallback *next = cb->next;
	if (entry->ref) ++entry->ref;
	cb->handler(cb->ctx, entry->color, entry->pixel);
	free(cb);
	cb = next;
    }
    entry->callbacks = 0;
}

void X11Adapter_mapColor(void *ctx, MapColorHandler handler, Color color)
{
    color |= 0xffU; // alpha is not supported for X11 core colors
    ColorMapEntry *next = 0;
    ColorMapEntry *reuse = 0;
    for (unsigned i = 0; i < MAXCOLORS; ++i)
    {
	if (colorMap[i].ref && colorMap[i].color == color)
	{
	    if (colorMap[i].callbacks)
	    {
		ColorMapCallback *cb = PSC_malloc(sizeof *cb);
		cb->next = colorMap[i].callbacks;
		cb->ctx = ctx;
		cb->handler = handler;
		colorMap[i].callbacks = cb;
		return;
	    }
	    ++colorMap[i].ref;
	    handler(ctx, color, colorMap[i].pixel);
	    return;
	}
	else
	{
	    if (!next && !colorMap[i].ref) next = colorMap + i;
	    if (!reuse && colorMap[i].ref == 1) reuse = colorMap + i;
	}
    }
    if (!next)
    {
	if (!reuse)
	{
	    PSC_Log_msg(PSC_L_WARNING, "X11Adapter: color map full");
	    return;
	}
	next = reuse;
	CHECK(xcb_free_colors(c, s->default_colormap, 0, 1, &next->pixel),
		"X11Adapter: cannot free allocated color", 0);
    }
    next->callbacks = PSC_malloc(sizeof *next->callbacks);
    next->callbacks->next = 0;
    next->callbacks->ctx = ctx;
    next->callbacks->handler = handler;
    next->color = color;
    next->ref = 1;
    AWAIT(xcb_alloc_color(c, s->default_colormap, Color_red16(color),
		Color_green16(color), Color_blue16(color)),
	    next, receiveColor);
}

void X11Adapter_unmapColor(uint32_t pixel)
{
    for (unsigned i = 0; i < MAXCOLORS; ++i)
    {
	if (colorMap[i].pixel == pixel && colorMap[i].ref > 1)
	{
	    --colorMap[i].ref;
	    return;
	}
    }
}

void X11Adapter_done(void)
{
    if (!c) return;
    uint32_t pixels[MAXCOLORS];
    unsigned npixels = 0;
    for (unsigned i = 0; i < MAXCOLORS; ++i)
    {
	if (colorMap[i].callbacks)
	{
	    ColorMapCallback *cb = colorMap[i].callbacks;
	    while (cb)
	    {
		ColorMapCallback *next = cb->next;
		free(cb);
		cb = next;
	    }
	}
	if (colorMap[i].ref) pixels[npixels++] = colorMap[i].pixel;
    }
    if (npixels) xcb_free_colors(c, s->default_colormap, 0, npixels, pixels);
    memset(colorMap, 0, MAXCOLORS * sizeof *colorMap);
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
    PSC_Event_destroy(selectionRequest);
    PSC_Event_destroy(selectionNotify);
    PSC_Event_destroy(selectionClear);
    PSC_Event_destroy(propertyNotify);
    PSC_Event_destroy(motionNotify);
    PSC_Event_destroy(mapNotify);
    PSC_Event_destroy(leave);
    PSC_Event_destroy(keypress);
    PSC_Event_destroy(focusout);
    PSC_Event_destroy(focusin);
    PSC_Event_destroy(expose);
    PSC_Event_destroy(enter);
    PSC_Event_destroy(configureNotify);
    PSC_Event_destroy(clientmsg);
    PSC_Event_destroy(buttonrelease);
    PSC_Event_destroy(buttonpress);
    eventsDone = 0;
    requestError = 0;
    unmapNotify = 0;
    selectionRequest = 0;
    selectionClear = 0;
    propertyNotify = 0;
    motionNotify = 0;
    mapNotify = 0;
    leave = 0;
    keypress = 0;
    focusout = 0;
    focusin = 0;
    expose = 0;
    enter = 0;
    configureNotify = 0;
    clientmsg = 0;
    buttonrelease = 0;
    buttonpress = 0;
    rootformat = 0;
    memset(formats, 0, sizeof formats);
    for (unsigned i = 0; i < sizeof cursornames / sizeof *cursornames; ++i)
    {
	xcb_free_cursor(c, cursors[i]);
    }
    if (cctx) xcb_cursor_context_free(cctx);
    cctx = 0;
    XRdb_destroy(rdb);
    rdb = 0;
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
    dpi = 96.;
    s = 0;
    c = 0;
    memset(atoms, 0, sizeof atoms);
    memset(cursors, 0, sizeof cursors);
}


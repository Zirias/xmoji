#ifndef XMOJI_X11ADAPTER_H
#define XMOJI_X11ADAPTER_H

#include "macros.h"

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(XRdb);
struct xkb_compose_table;

typedef enum XGlitch
{
    XG_NONE		    = 0,
    XG_RENDER_SRC_OFFSET    = 1 << 0
} XGlitch;

#define X_ATOMS(X) \
    X(ATOM_PAIR) \
    X(CLIPBOARD) \
    X(INCR) \
    X(MULTIPLE) \
    X(TIMESTAMP) \
    X(TARGETS) \
    X(TEXT) \
    X(UTF8_STRING) \
    X(WM_CHANGE_STATE) \
    X(WM_CLASS) \
    X(WM_DELETE_WINDOW) \
    X(WM_PROTOCOLS) \
    X(WM_STATE) \
    X(_NET_WM_ICON_NAME) \
    X(_NET_WM_NAME) \
    X(_NET_WM_STATE) \
    X(_NET_WM_STATE_HIDDEN) \
    X(_NET_WM_STATE_SKIP_TASKBAR) \
    X(_NET_WM_WINDOW_TYPE) \
    X(_NET_WM_WINDOW_TYPE_NORMAL) \
    X(_NET_WM_WINDOW_TYPE_DIALOG) \
    X(_NET_WM_WINDOW_TYPE_UTILITY)

typedef enum XAtomId
{
    X_ATOMS(X_ENUM)
    NATOMS
} XAtomId;

#define WM_STATE_WITHDRAWN  0
#define WM_STATE_NORMAL	    1
#define WM_STATE_ICONIC	    3

typedef enum WMHintFlags
{
    WM_HINT_INPUT	    = 1 << 0,
    WM_HINT_STATE	    = 1 << 1,
    WM_HINT_ICON_PIXMAP	    = 1 << 2,
    WM_HINT_ICON_WINDOW	    = 1 << 3,
    WM_HINT_ICON_MASK	    = 1 << 4,
    WM_HINT_WINDOW_GROUP    = 1 << 5,
    WM_HINT_MESSAGE	    = 1 << 6,
    WM_HINT_URGENCY	    = 1 << 7
} WMHintFlags;

typedef struct WMHints
{
    uint32_t flags;
    uint32_t input;
    uint32_t state;
    uint32_t icon_pixmap;
    uint32_t icon_window;
    int32_t icon_x, icon_y;
    uint32_t icon_mask;
    uint32_t window_group;
} WMHints;

typedef enum WMSizeHintFlags
{
    WM_SIZE_HINT_US_POSITION	= 1 << 0,
    WM_SIZE_HINT_US_SIZE	= 1 << 1,
    WM_SIZE_HINT_P_POSITION	= 1 << 2,
    WM_SIZE_HINT_P_SIZE		= 1 << 3,
    WM_SIZE_HINT_P_MIN_SIZE	= 1 << 4,
    WM_SIZE_HINT_P_MAX_SIZE	= 1 << 5,
    WM_SIZE_HINT_P_RESIZE_INC	= 1 << 6,
    WM_SIZE_HINT_P_ASPECT	= 1 << 7,
    WM_SIZE_HINT_BASE_SIZE	= 1 << 8,
    WM_SIZE_HINT_P_WIN_GRAVITY	= 1 << 9
} WMSizeHintFlags;

typedef struct WMSizeHints
{
    uint32_t flags;
    int32_t x, y;
    int32_t width, height;
    int32_t min_width, min_height;
    int32_t max_width, max_height;
    int32_t width_inc, height_inc;
    int32_t min_aspect_num, min_aspect_den;
    int32_t max_aspect_num, max_aspect_den;
    int32_t base_width, base_height;
    uint32_t win_gravity;
} WMSizeHints;

typedef enum XkbModifier
{
    XM_NONE	    = 0,
    XM_SHIFT	    = 1 << 0,
    XM_CAPSLOCK	    = 1 << 1,
    XM_CONTROL	    = 1 << 2,
    XM_ALT	    = 1 << 3,
    XM_NUMLOCK	    = 1 << 4,
    XM_LOGO	    = 1 << 5
} XkbModifier;

typedef struct XkbKeyEventArgs
{
    uint32_t keycode;
    uint32_t keysym;
    XkbModifier modifiers;
    XkbModifier rawmods;
} XkbKeyEventArgs;

#ifdef TRACE_X11_REQUESTS
typedef struct X11RequestId
{
    const char *reqsource;
    const char *sourcefile;
    const char *function;
    unsigned lineno;
    unsigned sequence;
} X11RequestId;
#include <poser/core/log.h>
#define priv_Trace(x,s) ( \
	PSC_Log_fmt(PSC_L_DEBUG, __FILE__ ":" STR(__LINE__) \
	    ":%s: " s, __func__), \
	(X11RequestId) { \
	    .reqsource = s, \
	    .sourcefile = __FILE__, \
	    .function = __func__, \
	    .lineno = __LINE__, \
	    .sequence = (x).sequence })
#else
typedef unsigned X11RequestId;
#define priv_Trace(x,s) (x).sequence
#endif

typedef struct RequestErrorEventArgs
{
    X11RequestId reqid;
    uint16_t opMinor;
    uint8_t opMajor;
    uint8_t code;
} RequestErrorEventArgs;

typedef enum XCursor
{
    XC_LEFTPTR,
    XC_XTERM,
    XC_HAND
} XCursor;

typedef void (*X11ReplyHandler)(void *ctx, unsigned sequence, void *reply,
	xcb_generic_error_t *error);

#define A(x) (X11Adapter_atom(x))

/** Await the result of an X11 request
 *
 * SYNOPSIS: AWAIT(cookie, void *context, X11ReplyHandler callback)
 *
 * Register a callback for completion of an xcb request. The callback is
 * guaranteed to be called, even if there is neither a reply nor an error.
 * Therefore, when passed an xcb_void_cookie_t, it might inject a dummy
 * GetInputFocus request to "sync" the X11 connection if there isn't a request
 * with an expected reply following.
 *
 * cookie	Any xcb cookie type, identifying the request
 * context	Context for completion callback (typically an object instance)
 * callback	Called when the request completed
 */
#define AWAIT(x,c,h) _Generic((x), \
	    xcb_void_cookie_t: X11Adapter_awaitNoreply, \
	    default: X11Adapter_await \
	)(priv_Trace(x,#x), (c), (h))

/** Check an X11 request for errors
 *
 * SYNOPSIS: CHECK(cookie, void *context, X11ReplyHandler callback)
 *           CHECK(cookie, const char *msg, 0)
 *           CHECK(cookie, const char *fmt, unsigned id)
 *           CHECK(cookie, const char *fmt, const char *str)
 *
 * Check a request for errors, should only be used with xcb_void_cookie_t.
 * Errors are found both as xcb events and when checking for xcb replies, so
 * it doesn't matter whether the cookie is obtained from a "_checked" request
 * variant or not.
 *
 * If a callback is given, it will be called on errors caused by the request.
 * Otherwise, an error-level log message is created when an error for the
 * request is received.
 *
 * cookie	An xcb_void_cookie_t
 * msg		A log message for the error case
 * fmt		A format string for the error log message, containing a
 *		conversion for an argument of type (unsigned) or (const char *)
 * id		An identifier. When given, it can be used for printing in
 *		`fmt`, but will also be used for firing a generic
 *		`requestError` event than can be registered using this same
 *		identifier. By using a value obtained from xcb_generate_id()
 *		here, this allows for a generic error notification without
 *		an explicit callback for the request.
 * str		A string argument for the error log message, only useful for
 *		logging
 */
#define CHECK(x,c,h) _Generic((h), \
	    X11ReplyHandler: X11Adapter_check, \
	    char *: X11Adapter_checkLogString, \
	    unsigned: X11Adapter_checkLogUnsigned, \
	    int: X11Adapter_checkLogString \
	)(priv_Trace(x,#x), (c), (h))

int X11Adapter_init(int argc, char **argv, const char *locale,
	const char *name, const char *classname)
    ATTR_NONNULL((3)) ATTR_NONNULL((4));

xcb_connection_t *X11Adapter_connection(void);
xcb_screen_t *X11Adapter_screen(void);
XRdb *X11Adapter_resources(void);
XGlitch X11Adapter_glitches(void);
size_t X11Adapter_maxRequestSize(void);
xcb_atom_t X11Adapter_atom(XAtomId id) ATTR_PURE;
xcb_render_pictformat_t X11Adapter_rootformat(void);
xcb_render_pictformat_t X11Adapter_alphaformat(void);
xcb_render_pictformat_t X11Adapter_argbformat(void);
xcb_render_pictformat_t X11Adapter_rgbformat(void);
struct xkb_compose_table *X11Adapter_kbdcompose(void);
PSC_Event *X11Adapter_buttonpress(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_buttonrelease(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_clientmsg(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_configureNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_enter(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_expose(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_focusin(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_focusout(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_keypress(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_leave(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_mapNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_motionNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_propertyNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_selectionClear(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_selectionNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_selectionRequest(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_unmapNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_requestError(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_eventsDone(void) ATTR_RETNONNULL;
const char *X11Adapter_wmClass(size_t *sz) ATTR_RETNONNULL;
xcb_cursor_t X11Adapter_cursor(XCursor id);

unsigned X11Adapter_await(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_awaitNoreply(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_check(X11RequestId reqid, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_checkLogUnsigned(X11RequestId reqid, const char *msg,
	uint32_t arg)
    ATTR_NONNULL((2));
unsigned X11Adapter_checkLogString(X11RequestId reqid, const char *msg,
	const char *arg)
    ATTR_NONNULL((2));

void X11Adapter_done(void);

#endif

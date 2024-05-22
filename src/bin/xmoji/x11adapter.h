#ifndef XMOJI_X11ADAPTER_H
#define XMOJI_X11ADAPTER_H

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(PSC_Event);

#define _STR(x) #x
#define STR(x) _STR(x)

#define X_ENUM(a) a,
#define X_ATOMS(X) \
    X(UTF8_STRING) \
    X(WM_CLASS) \
    X(WM_DELETE_WINDOW) \
    X(WM_PROTOCOLS) \
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

typedef void (*X11ReplyHandler)(void *ctx, unsigned sequence, void *reply,
	xcb_generic_error_t *error);

#ifdef DEBUG
#include <poser/core/log.h>
#define priv_Trace(x) ( \
	PSC_Log_fmt(PSC_L_DEBUG, __FILE__ ":" STR(__LINE__) \
	    ":%s: " #x, __func__), \
	(x).sequence)
#else
#define priv_Trace(x) (x).sequence
#endif

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
	)(priv_Trace(x), (c), (h))

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
	)(priv_Trace(x), (c), (h))

int X11Adapter_init(int argc, char **argv, const char *classname);

xcb_connection_t *X11Adapter_connection(void);
xcb_screen_t *X11Adapter_screen(void);
size_t X11Adapter_maxRequestSize(void);
xcb_atom_t X11Adapter_atom(XAtomId id);
xcb_render_pictformat_t X11Adapter_rootformat(void);
xcb_render_pictformat_t X11Adapter_alphaformat(void);
xcb_render_pictformat_t X11Adapter_argbformat(void);
PSC_Event *X11Adapter_clientmsg(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_configureNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_expose(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_flushed(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_requestError(void) ATTR_RETNONNULL;
const char *X11Adapter_wmClass(size_t *sz) ATTR_RETNONNULL;
char *X11Adapter_toLatin1(const char *utf8) ATTR_NONNULL((1)) ATTR_RETNONNULL;

unsigned X11Adapter_await(unsigned sequence, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_awaitNoreply(unsigned sequence, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_check(unsigned sequence, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_checkLogUnsigned(unsigned sequence, const char *msg,
	uint32_t arg)
    ATTR_NONNULL((2));
unsigned X11Adapter_checkLogString(unsigned sequence, const char *msg,
	const char *arg)
    ATTR_NONNULL((2));

void X11Adapter_done(void);

#endif

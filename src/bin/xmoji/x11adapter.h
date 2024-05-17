#ifndef XMOJI_X11ADAPTER_H
#define XMOJI_X11ADAPTER_H

#include <poser/decl.h>
#include <xcb/render.h>

C_CLASS_DECL(PSC_Event);

#define X_ENUM(a) a,
#define X_ATOMS(X) \
    X(UTF8_STRING) \
    X(WM_CLASS) \
    X(WM_DELETE_WINDOW) \
    X(WM_PROTOCOLS) \
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

typedef void (*X11ReplyHandler)(void *ctx, unsigned sequence, void *reply,
	xcb_generic_error_t *error);

#define A(x) (X11Adapter_atom(x))
#define AWAIT(x,c,h) (_Generic((x), \
	    xcb_void_cookie_t: X11Adapter_awaitNoreply, \
	    default: X11Adapter_await \
	)((x).sequence, c, h))

int X11Adapter_init(int argc, char **argv, const char *classname);

xcb_connection_t *X11Adapter_connection(void);
xcb_screen_t *X11Adapter_screen(void);
xcb_atom_t X11Adapter_atom(XAtomId id);
xcb_render_pictformat_t X11Adapter_rootformat(void);
xcb_render_pictformat_t X11Adapter_alphaformat(void);
xcb_render_pictformat_t X11Adapter_argbformat(void);
PSC_Event *X11Adapter_clientmsg(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_configureNotify(void) ATTR_RETNONNULL;
PSC_Event *X11Adapter_expose(void) ATTR_RETNONNULL;
const char *X11Adapter_wmClass(size_t *sz) ATTR_RETNONNULL;
char *X11Adapter_toLatin1(const char *utf8) ATTR_NONNULL((1)) ATTR_RETNONNULL;

unsigned X11Adapter_await(unsigned sequence, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));
unsigned X11Adapter_awaitNoreply(unsigned sequence, void *ctx,
	X11ReplyHandler handler) ATTR_NONNULL((3));

void X11Adapter_done(void);

#endif

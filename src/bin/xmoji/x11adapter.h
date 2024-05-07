#ifndef XMOJI_X11ADAPTER_H
#define XMOJI_X11ADAPTER_H

#include <poser/decl.h>
#include <xcb/xcb.h>

C_CLASS_DECL(X11Adapter);
C_CLASS_DECL(PSC_Event);

#define X_ENUM(a) a,
#define X_ATOMS(X) \
    X(WM_DELETE_WINDOW) \
    X(WM_PROTOCOLS) \
    X(_NET_WM_STATE) \
    X(_NET_WM_STATE_HIDDEN) \
    X(_NET_WM_STATE_SKIP_TASKBAR)

typedef enum XAtomId
{
    X_ATOMS(X_ENUM)
    NATOMS
} XAtomId;

#define A(x) (X11Adapter_atom(x11, (x)))

X11Adapter *X11Adapter_create(void);

xcb_connection_t *X11Adapter_connection(X11Adapter *self)
    CMETHOD;
xcb_screen_t *X11Adapter_screen(X11Adapter *self)
    CMETHOD;
xcb_atom_t X11Adapter_atom(X11Adapter *self, XAtomId id)
    CMETHOD;
PSC_Event *X11Adapter_clientmsg(X11Adapter *self)
    CMETHOD ATTR_RETNONNULL;

void X11Adapter_destroy(X11Adapter *self);

#endif

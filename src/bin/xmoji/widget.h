#ifndef XMOJI_WIDGET_H
#define XMOJI_WIDGET_H

#include "colorset.h"
#include "object.h"
#include "valuetypes.h"
#include "x11adapter.h"
#include "xselection.h"

#include <poser/decl.h>
#include <xcb/render.h>

typedef struct KeyEvent
{
    uint32_t codepoint;
    uint32_t keysym;
    XkbModifier modifiers;
} KeyEvent;

typedef enum MouseButton
{
    MB_NONE	    = 0,
    MB_LEFT	    = 1 << 0,
    MB_MIDDLE	    = 1 << 1,
    MB_RIGHT	    = 1 << 2,
    MB_WHEEL_UP	    = 1 << 3,
    MB_WHEEL_DOWN   = 1 << 4
} MouseButton;

typedef struct ClickEvent
{
    MouseButton button;
    Pos pos;
    int dblclick;
} ClickEvent;

typedef struct DragEvent
{
    MouseButton button;
    Pos from;
    Pos to;
} DragEvent;

typedef struct MetaWidget
{
    MetaObject base;
    void (*expose)(void *widget, Rect region);
    int (*draw)(void *widget, xcb_render_picture_t picture);
    int (*show)(void *widget);
    int (*hide)(void *widget);
    int (*activate)(void *widget);
    int (*deactivate)(void *widget);
    void (*enter)(void *widget);
    void (*leave)(void *widget);
    void (*focus)(void *widget);
    void (*unfocus)(void *widget);
    void (*paste)(void *widget, XSelectionContent content);
    void (*unselect)(void *widget);
    void *(*childAt)(void *widget, Pos pos);
    Size (*minSize)(const void *widget);
    void (*keyPressed)(void *widget, const KeyEvent *event);
    int (*clicked)(void *widget, const ClickEvent *event);
    void (*dragged)(void *widget, const DragEvent *event);
} MetaWidget;

#define MetaWidget_init(mexpose, mdraw, mshow, mhide, \
	mactivate, mdeactivate, menter, mleave, mfocus, munfocus, \
	mpaste, munselect, mchildAt, mminSize, mkeyPressed, \
	mclicked, mdragged, \
	...) { \
    .base = MetaObject_init(__VA_ARGS__), \
    .expose = mexpose, \
    .draw = mdraw, \
    .show = mshow, \
    .hide = mhide, \
    .activate = mactivate, \
    .deactivate = mdeactivate, \
    .enter = menter, \
    .leave = mleave, \
    .focus = mfocus, \
    .unfocus = munfocus, \
    .paste = mpaste, \
    .unselect = munselect, \
    .childAt = mchildAt, \
    .minSize = mminSize, \
    .keyPressed = mkeyPressed, \
    .clicked = mclicked, \
    .dragged = mdragged \
}

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(Widget);

typedef struct WidgetEventArgs
{
    int external;
} WidgetEventArgs;

typedef struct SizeChangedEventArgs
{
    int external;
    Size oldSize;
    Size newSize;
} SizeChangedEventArgs;

Widget *Widget_createBase(void *derived, const char *name, void *parent);
#define Widget_create(...) Widget_createBase(0, __VA_ARGS__)
const char *Widget_name(const void *self) CMETHOD;
const char *Widget_resname(const void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_shown(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_hidden(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_activated(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeRequested(void *self) CMETHOD ATTR_RETNONNULL;
PSC_Event *Widget_sizeChanged(void *self) CMETHOD ATTR_RETNONNULL;
Widget *Widget_container(const void *self) CMETHOD;
void Widget_setContainer(void *self, void *container) CMETHOD;
int Widget_draw(void *self) CMETHOD;
int Widget_show(void *self) CMETHOD;
int Widget_hide(void *self) CMETHOD;
void Widget_activate(void *self) CMETHOD;
void Widget_deactivate(void *self) CMETHOD;
int Widget_active(const void *self) CMETHOD;
void *Widget_enterAt(void *self, Pos pos) CMETHOD;
void Widget_leave(void *self) CMETHOD;
void Widget_acceptFocus(void *self, int accept) CMETHOD;
void Widget_focus(void *self) CMETHOD;
void Widget_unfocus(void *self) CMETHOD;
int Widget_focused(const void *self) CMETHOD;
void Widget_unselect(void *self) CMETHOD;
void Widget_setSize(void *self, Size size) CMETHOD;
void Widget_setMaxSize(void *self, Size maxSize) CMETHOD;
Size Widget_minSize(const void *self) CMETHOD;
Size Widget_size(const void *self) CMETHOD;
void Widget_setPadding(void *self, Box padding) CMETHOD;
Box Widget_padding(const void *self) CMETHOD;
void Widget_setAlign(void *self, Align align) CMETHOD;
Align Widget_align(const void *self) CMETHOD;
void Widget_setCursor(void *self, XCursor cursor) CMETHOD;
XCursor Widget_cursor(const void *self) CMETHOD;
void Widget_setOrigin(void *self, Pos origin) CMETHOD;
Rect Widget_geometry(const void *self) CMETHOD;
Pos Widget_origin(const void *self) CMETHOD;
Pos Widget_contentOrigin(const void *self, Size contentSize) CMETHOD;
const ColorSet *Widget_colorSet(const void *self) CMETHOD;
Color Widget_color(const void *self, ColorRole role) CMETHOD;
void Widget_setColor(void *self, ColorRole role, Color color) CMETHOD;
void Widget_setBackground(void *self, int enabled, ColorRole role) CMETHOD;
xcb_drawable_t Widget_drawable(const void *self) CMETHOD;
int Widget_visible(const void *self) CMETHOD;
void Widget_keyPressed(void *self, const KeyEvent *event) CMETHOD;
int Widget_clicked(void *self, const ClickEvent *event) CMETHOD;
void Widget_dragged(void *self, const DragEvent *event) CMETHOD;

// "protected" API meant only for derived classes
void Widget_setDrawable(void *self, xcb_drawable_t drawable) CMETHOD;
void Widget_requestSize(void *self) CMETHOD;
void Widget_invalidate(void *self) CMETHOD;
void Widget_invalidateRegion(void *self, Rect region) CMETHOD;
void Widget_disableDrawing(void *self) CMETHOD;
void Widget_setWindowSize(void *self, Size size) CMETHOD;
void Widget_showWindow(void *self) CMETHOD;
void Widget_hideWindow(void *self) CMETHOD;
void Widget_requestPaste(void *self, XSelectionName name,
	XSelectionType type) CMETHOD;
void Widget_setSelection(void *self, XSelectionName name,
	XSelectionContent content) CMETHOD;
const Rect *Widget_damages(const void *self, int *num) CMETHOD;

#endif

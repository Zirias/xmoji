#include "widget.h"

#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

#define MAXDAMAGES 16
static void destroy(void *obj);
static int show(void *obj);
static int hide(void *obj);
static Size minSize(const void *obj);

static MetaWidget mo = MetaWidget_init("Widget",
	destroy, 0, 0, show, hide, minSize, 0);

struct Widget
{
    Object base;
    PSC_Event *shown;
    PSC_Event *hidden;
    PSC_Event *sizeRequested;
    PSC_Event *sizeChanged;
    Widget *parent;
    ColorSet *colorSet;
    Rect geometry;
    Rect clip;
    Rect damages[MAXDAMAGES];
    Box padding;
    Size maxSize;
    xcb_drawable_t drawable;
    xcb_render_picture_t picture;
    xcb_render_picture_t bgpicture;
    ColorRole backgroundRole;
    Align align;
    InputEvents events;
    int drawBackground;
    int ndamages;
    int visible;
};

static void destroy(void *obj)
{
    Widget *self = obj;
    ColorSet_destroy(self->colorSet);
    PSC_Event_destroy(self->sizeChanged);
    PSC_Event_destroy(self->sizeRequested);
    PSC_Event_destroy(self->hidden);
    PSC_Event_destroy(self->shown);
    free(self);
}

static int doshow(Widget *self, int external)
{
    if (!self->visible)
    {
	self->visible = 1;
	Widget_invalidate(self);
	WidgetEventArgs args = { external };
	PSC_Event_raise(self->shown, 0, &args);
    }
    return 0;
}

static int show(void *obj)
{
    return doshow(Object_instance(obj), 0);
}

static int dohide(Widget *self, int external)
{
    if (self->visible)
    {
	self->visible = 0;
	self->ndamages = 0;
	WidgetEventArgs args = { external };
	PSC_Event_raise(self->hidden, 0, &args);
    }
    return 0;
}

static int hide(void *obj)
{
    return dohide(Object_instance(obj), 0);
}

static Size minSize(const void *obj)
{
    (void)obj;
    return (Size){0, 0};
}

Widget *Widget_createBase(void *derived, void *parent, InputEvents events)
{
    REGTYPE(0);

    Widget *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    if (!derived) derived = self;
    self->base.base = Object_create(derived);
    self->base.type = OBJTYPE;
    self->shown = PSC_Event_create(self);
    self->hidden = PSC_Event_create(self);
    self->sizeRequested = PSC_Event_create(self);
    self->sizeChanged = PSC_Event_create(self);
    self->parent = parent;
    self->padding = (Box){ 3, 3, 3, 3 };
    self->maxSize = (Size){ -1, -1 };
    self->events = events;

    if (parent) Object_own(parent, derived);
    else self->colorSet = ColorSet_create(0xffffffff, 0x000000ff);
    return self;
}

PSC_Event *Widget_shown(void *self)
{
    Widget *w = Object_instance(self);
    return w->shown;
}

PSC_Event *Widget_hidden(void *self)
{
    Widget *w = Object_instance(self);
    return w->hidden;
}

PSC_Event *Widget_sizeRequested(void *self)
{
    Widget *w = Object_instance(self);
    return w->sizeRequested;
}

PSC_Event *Widget_sizeChanged(void *self)
{
    Widget *w = Object_instance(self);
    return w->sizeChanged;
}

Widget *Widget_parent(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->parent;
}

static void setContentClipArea(Widget *self, xcb_connection_t *c)
{
    Rect contentArea = Rect_pad(self->geometry, self->padding);
    if (self->ndamages < 0)
    {
	xcb_rectangle_t cliprect = {
	    contentArea.pos.x, contentArea.pos.y,
	    contentArea.size.width, contentArea.size.height
	};
	CHECK(xcb_render_set_picture_clip_rectangles(c,
		    self->picture, 0, 0, 1, &cliprect),
		"Cannot set clipping region on 0x%x",
		(unsigned)self->drawable);
    }
    else
    {
	Box contentBox = Box_fromRect(contentArea);
	xcb_rectangle_t cliprects[MAXDAMAGES];
	int i = 0;
	for (int d = 0; d < self->ndamages; ++d)
	{
	    if (!Rect_overlaps(contentArea, self->damages[d])) continue;
	    Box damageBox = Box_fromRect(self->damages[d]);
	    if (damageBox.left < contentBox.left)
		damageBox.left = contentBox.left;
	    if (damageBox.top < contentBox.top)
		damageBox.top = contentBox.top;
	    if (damageBox.right > contentBox.right)
		damageBox.right = contentBox.right;
	    if (damageBox.bottom > contentBox.bottom)
		damageBox.bottom = contentBox.bottom;
	    cliprects[i++] = (xcb_rectangle_t){
		damageBox.left, damageBox.top,
		    damageBox.right - damageBox.left,
		    damageBox.bottom - damageBox.top };
	}
	CHECK(xcb_render_set_picture_clip_rectangles(c,
		    self->picture, 0, 0, i, cliprects),
		"Cannot set clipping region on 0x%x",
		(unsigned)self->drawable);
    }
}

int Widget_draw(void *self)
{
    Widget *w = Object_instance(self);
    if (!w->drawable) return -1;
    if (!Widget_visible(self)) return 0;
    int rc = -1;
    if (!w->ndamages)
    {
	Object_vcall(rc, Widget, draw, self, 0);
	return rc;
    }
    xcb_connection_t *c = X11Adapter_connection();
    if (w->drawBackground)
    {
	if (memcmp(&w->geometry, &w->clip, sizeof w->geometry))
	{
	    w->clip = w->geometry;
	    xcb_rectangle_t clip = {0, 0,
		w->clip.size.width, w->clip.size.height};
	    CHECK(xcb_render_set_picture_clip_rectangles(c,
			w->bgpicture, w->clip.pos.x, w->clip.pos.y, 1, &clip),
		    "Cannot set clipping region on 0x%x",
		    (unsigned)w->drawable);
	}
	Color color = Widget_color(w, w->backgroundRole);
	if (w->ndamages < 0)
	{
	    xcb_rectangle_t rect = {w->geometry.pos.x, w->geometry.pos.y,
		w->geometry.size.width, w->geometry.size.height};
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			w->bgpicture, Color_xcb(color), 1, &rect),
		    "Cannot draw widget background on 0x%x",
		    (unsigned)w->picture);
	}
	else for (int d = 0; d < w->ndamages; ++d)
	{
	    xcb_rectangle_t rect = {w->damages[d].pos.x, w->damages[d].pos.y,
		w->damages[d].size.width, w->damages[d].size.height};
	    CHECK(xcb_render_fill_rectangles(c, XCB_RENDER_PICT_OP_OVER,
			w->bgpicture, Color_xcb(color), 1, &rect),
		    "Cannot draw widget background on 0x%x",
		    (unsigned)w->picture);
	}
    }
    setContentClipArea(w, c);
    Object_vcall(rc, Widget, draw, self, w->picture);
    w->ndamages = 0;
    return rc;
}

int Widget_show(void *self)
{
    int rc = -1;
    Object_vcall(rc, Widget, show, self);
    return rc;
}

int Widget_hide(void *self)
{
    int rc = -1;
    Object_vcall(rc, Widget, hide, self);
    return rc;
}

static void setSize(Widget *self, int external, Size size)
{
    if (size.width > self->maxSize.width) size.width = self->maxSize.width;
    if (size.height > self->maxSize.height) size.height = self->maxSize.height;
    if (memcmp(&self->geometry.size, &size, sizeof size))
    {
	SizeChangedEventArgs args = { external, self->geometry.size, size };
	self->geometry.size = size;
	Widget_invalidate(self);
	PSC_Event_raise(self->sizeChanged, 0, &args);
    }
}

void Widget_setSize(void *self, Size size)
{
    setSize(Object_instance(self), 0, size);
}

void Widget_setMaxSize(void *self, Size maxSize)
{
    Widget *w = Object_instance(self);
    w->maxSize = maxSize;
    setSize(w, 0, w->geometry.size);
}

Size Widget_minSize(const void *self)
{
    const Widget *w = Object_instance(self);
    Size size = { 0, 0 };
    Object_vcall(size, Widget, minSize, w);
    if (size.width || size.height)
    {
	size.width += w->padding.left + w->padding.right;
	size.height += w->padding.top + w->padding.bottom;
    }
    return size;
}

Size Widget_size(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->geometry.size;
}

void Widget_setPadding(void *self, Box padding)
{
    Widget *w = Object_instance(self);
    w->padding = padding;
}

Box Widget_padding(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->padding;
}

void Widget_setAlign(void *self, Align align)
{
    Widget *w = Object_instance(self);
    w->align = align;
}

Align Widget_align(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->align;
}

void Widget_setOrigin(void *self, Pos pos)
{
    Widget *w = Object_instance(self);
    w->geometry.pos = pos;
}

Rect Widget_geometry(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->geometry;
}

Pos Widget_origin(const void *self)
{
    const Widget *w = Object_instance(self);
    return w->geometry.pos;
}

Pos Widget_contentOrigin(const void *self, Size contentSize)
{
    const Widget *w = Object_instance(self);
    contentSize.width += w->padding.left + w->padding.right;
    contentSize.height += w->padding.top + w->padding.bottom;
    Pos contentPos = w->geometry.pos;
    int hdiff = w->geometry.size.width - contentSize.width;
    if (hdiff > 0)
    {
	if (w->align & AH_RIGHT) contentPos.x += hdiff;
	else if (w->align & AH_CENTER) contentPos.x += hdiff / 2;
    }
    int vdiff = w->geometry.size.height - contentSize.height;
    if (vdiff > 0)
    {
	if (w->align & AV_BOTTOM) contentPos.y += vdiff;
	else if (w->align & AV_MIDDLE) contentPos.y += vdiff / 2;
    }
    contentPos.x += w->padding.left;
    contentPos.y += w->padding.top;
    return contentPos;
}

const ColorSet *Widget_colorSet(const void *self)
{
    const Widget *w = Object_instance(self);
    if (w->colorSet) return w->colorSet;
    if (w->parent) return Widget_colorSet(w->parent);
    return 0;
}

Color Widget_color(const void *self, ColorRole role)
{
    const ColorSet *colorSet = Widget_colorSet(self);
    if (!colorSet) return 0x00000000;
    return ColorSet_color(colorSet, role);
}

void Widget_setColor(void *self, ColorRole role, Color color)
{
    Widget *w = Object_instance(self);
    if (!w->colorSet)
    {
	const ColorSet *cs = Widget_colorSet(self);
	if (cs) w->colorSet = ColorSet_clone(cs);
	else w->colorSet = ColorSet_create(0xffffffff, 0x000000ff);
    }
    ColorSet_setColor(w->colorSet, role, color);
}

void Widget_setBackground(void *self, int enabled, ColorRole role)
{
    Widget *w = Object_instance(self);
    w->backgroundRole = role;
    w->drawBackground = enabled;
}

xcb_drawable_t Widget_drawable(const void *self)
{
    Widget *w = Object_instance(self);
    return w->drawable;
}

void Widget_setDrawable(void *self, xcb_drawable_t drawable)
{
    Widget *w = Object_instance(self);
    xcb_connection_t *c = X11Adapter_connection();
    if (w->drawable)
    {
	xcb_render_free_picture(c, w->bgpicture);
	xcb_render_free_picture(c, w->picture);
    }
    if (drawable)
    {
	w->drawable = drawable;
	w->picture = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, w->picture, drawable,
		    X11Adapter_rootformat(), 0, 0),
		"Cannot create XRender picture for 0x%x", (unsigned)drawable);
	w->bgpicture = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, w->bgpicture, drawable,
		    X11Adapter_rootformat(), 0, 0),
		"Cannot create XRender picture for 0x%x", (unsigned)drawable);
    }
    else
    {
	w->drawable = 0;
	w->picture = 0;
	w->bgpicture = 0;
    }
}

int Widget_visible(const void *self)
{
    const Widget *w = Object_instance(self);
    if (!w->drawable) return 0;
    if (!w->visible) return 0;
    if (!w->parent) return 1;
    return Widget_visible(w->parent);
}

void Widget_keyPressed(void *self, const KeyEvent *event)
{
    Widget *w = Object_instance(self);
    if (!(w->events & IE_KEYPRESSED)) return;
    Object_vcallv(Widget, keyPressed, w, event);
}

void Widget_requestSize(void *self)
{
    Widget *w = Object_instance(self);
    PSC_Event_raise(w->sizeRequested, 0, 0);
}

void Widget_invalidateRegion(void *self, Rect region)
{
    if (!Widget_visible(self)) return;
    Widget *w = Object_instance(self);
    if (w->ndamages < 0) return;
    if (!Rect_overlaps(region, w->geometry)) return;
    if (w->ndamages == MAXDAMAGES || Rect_contains(region, w->geometry))
    {
	w->ndamages = -1;
    }
    else
    {
	w->damages[w->ndamages++] = region;
    }
    Object_vcallv(Widget, expose, w, region);
}

void Widget_invalidate(void *self)
{
    if (!Widget_visible(self)) return;
    Widget *w = Object_instance(self);
    if (w->ndamages < 0) return;
    w->ndamages = -1;
    Object_vcallv(Widget, expose, w, w->geometry);
}

void Widget_setWindowSize(void *self, Size size)
{
    setSize(Object_instance(self), 1, size);
}

void Widget_showWindow(void *self)
{
    doshow(Object_instance(self), 1);
}

void Widget_hideWindow(void *self)
{
    dohide(Object_instance(self), 1);
}


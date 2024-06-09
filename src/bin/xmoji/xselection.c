#include "xselection.h"

#include "object.h"
#include "unistr.h"
#include "window.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

struct XSelection
{
    Window *w;
    Object *requestor;
    XSelectionCallback received;
    XSelectionContent content;
    XSelectionType requestType;
    xcb_atom_t name;
};

static void received(XSelection *self, XSelectionContent content)
{
    self->received(self->requestor, content);
    Object_destroy(self->requestor);
    self->requestor = 0;
    self->received = 0;
}

static void nothingReceived(XSelection *self)
{
    received(self, (XSelectionContent){0, XST_NONE});
}

static void onSelectionConverted(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    XSelection *self = obj;
    if (error)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Requesting selection failed for 0x%x",
		(unsigned)Window_id(self->w));
	nothingReceived(self);
    }
}

static void onSelectionReceived(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    XSelection *self = obj;
    if (!self->requestor) return;
    if (error)
    {
	PSC_Log_fmt(PSC_L_DEBUG, "Reading selection failed on 0x%x",
		(unsigned)Window_id(self->w));
	nothingReceived(self);
    }
    else if (reply)
    {
	xcb_get_property_reply_t *prop = reply;
	uint32_t len = xcb_get_property_value_length(prop);
	if (prop->type == XCB_ATOM_ATOM)
	{
	    xcb_atom_t reqtgt = XCB_ATOM_NONE;
	    xcb_atom_t *targets = xcb_get_property_value(prop);
	    for (uint32_t i = 0; i < len; ++i)
	    {
		if (targets[i] == A(UTF8_STRING))
		{
		    reqtgt = targets[i];
		    break;
		}
		if (targets[i] == XCB_ATOM_STRING) reqtgt = targets[i];
	    }
	    if (reqtgt == XCB_ATOM_NONE)
	    {
		PSC_Log_fmt(PSC_L_DEBUG,
			"No suitable selection format offered to 0x%x",
			(unsigned)Window_id(self->w));
	    }
	    else
	    {
		AWAIT(xcb_convert_selection(X11Adapter_connection(),
			    Window_id(self->w), self->name, reqtgt, self->name,
			    XCB_CURRENT_TIME),
			self, onSelectionConverted);
	    }
	}
	else if (prop->type == XCB_ATOM_STRING)
	{
	    UniStr *data = UniStr_createFromLatin1(
		    xcb_get_property_value(prop), len);
	    received(self, (XSelectionContent){data, XST_TEXT});
	    UniStr_destroy(data);
	}
	else if (prop->type == A(UTF8_STRING))
	{
	    char *utf8 = PSC_malloc(len+1);
	    memcpy(utf8, xcb_get_property_value(prop), len);
	    utf8[len] = 0;
	    UniStr *data = UniStr_create(utf8);
	    free(utf8);
	    received(self, (XSelectionContent){data, XST_TEXT});
	    UniStr_destroy(data);
	}
    }
    CHECK(xcb_delete_property(X11Adapter_connection(), Window_id(self->w),
		self->name),
	    "Cannot delete selection property on 0x%x",
	    (unsigned)Window_id(self->w));
}

static void selectionNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelection *self = receiver;
    if (!self->requestor) return;
    xcb_selection_notify_event_t *ev = args;
    if (ev->property != self->name) return;
    xcb_atom_t proptype = ev->target;
    if (proptype == A(TARGETS)) proptype = XCB_ATOM_ATOM;
    AWAIT(xcb_get_property(X11Adapter_connection(), 0, Window_id(self->w),
		XCB_ATOM_PRIMARY, proptype, 0, 64 * 1024),
	    self, onSelectionReceived);
}

static void selectionRequest(void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelection *self = receiver;
    xcb_selection_request_event_t *ev = args;
    if (ev->selection != self->name) return;
    xcb_selection_notify_event_t not = {
	.response_type = XCB_SELECTION_NOTIFY,
	.pad0 = 0,
	.sequence = 0,
	.time = ev->time,
	.requestor = ev->requestor,
	.selection = ev->selection,
	.target = ev->target,
	.property = ev->property
    };
    xcb_connection_t *c = X11Adapter_connection();
    if (self->content.type == XST_NONE)
    {
	not.property = XCB_ATOM_NONE;
    }
    else if (ev->target == A(TARGETS))
    {
	xcb_atom_t targets[] = { A(UTF8_STRING), A(TEXT), XCB_ATOM_STRING };
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, ev->requestor,
		    ev->property, XCB_ATOM_ATOM, 32,
		    sizeof targets / sizeof *targets, targets),
		"Cannot set property for selection request to 0x%x",
		(unsigned)Window_id(self->w));
    }
    else if (ev->target == XCB_ATOM_STRING)
    {
	char *str = UniStr_toLatin1(self->content.data);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, ev->requestor,
		    ev->property, XCB_ATOM_STRING, 8,
		    UniStr_len(self->content.data), str),
		"Cannot set property for selection request to 0x%x",
		(unsigned)Window_id(self->w));
	free(str);
    }
    else if (ev->target == A(TEXT) || ev->target == A(UTF8_STRING))
    {
	size_t len;
	char *str = UniStr_toUtf8(self->content.data, &len);
	CHECK(xcb_change_property(c, XCB_PROP_MODE_REPLACE, ev->requestor,
		    ev->property, A(UTF8_STRING), 8, len, str),
		"Cannot set property for selection request to 0x%x",
		(unsigned)Window_id(self->w));
	free(str);
    }
    else
    {
	not.property = XCB_ATOM_NONE;
    }
    CHECK(xcb_send_event(c, 0, ev->requestor, 0, (const char *)&not),
	    "Cannot send selection notification for 0x%x",
	    (unsigned)Window_id(self->w));
}

XSelection *XSelection_create(Window *w, XSelectionName name)
{
    XSelection *self = PSC_malloc(sizeof *self);
    xcb_atom_t nameatom;
    switch (name)
    {
	case XSN_PRIMARY:   nameatom = XCB_ATOM_PRIMARY; break;
	case XSN_CLIPBOARD: nameatom = A(CLIPBOARD); break;
	default:	    return 0;
    }
    memset(self, 0, sizeof *self);
    self->w = w;
    self->name = nameatom;
    PSC_Event_register(X11Adapter_selectionNotify(), self,
	    selectionNotify, Window_id(w));
    PSC_Event_register(X11Adapter_selectionRequest(), self,
	    selectionRequest, Window_id(w));
    return self;
}

void XSelection_request(XSelection *self, XSelectionType type,
	void *obj, XSelectionCallback received)
{
    if (self->requestor) nothingReceived(self);
    self->requestor = Object_ref(obj);
    self->received = received;
    self->requestType = type;
    AWAIT(xcb_convert_selection(X11Adapter_connection(), Window_id(self->w),
		self->name, A(TARGETS), self->name, XCB_CURRENT_TIME),
	    self, onSelectionConverted);
}

void XSelection_publish(XSelection *self, XSelectionContent content)
{
    switch (self->content.type)
    {
	case XST_TEXT:
	    UniStr_destroy(self->content.data);
	    break;

	default:
	    break;
    }
    switch (content.type)
    {
	case XST_TEXT:
	    self->content.data = UniStr_ref(content.data);
	    break;

	default:
	    self->content.data = 0;
	    break;
    }
    if (self->content.type == XST_NONE && content.type != XST_NONE)
    {
	CHECK(xcb_set_selection_owner(X11Adapter_connection(),
		    Window_id(self->w), self->name, XCB_CURRENT_TIME),
		"Cannot obtain selection ownership for 0x%x",
		(unsigned)Window_id(self->w));
    }
    self->content.type = content.type;
}

void XSelection_destroy(XSelection *self)
{
    if (!self) return;
    if (self->requestor) nothingReceived(self);
    PSC_Event_unregister(X11Adapter_selectionRequest(), self,
	    selectionRequest, Window_id(self->w));
    PSC_Event_unregister(X11Adapter_selectionNotify(), self,
	    selectionNotify, Window_id(self->w));
    free(self);
}


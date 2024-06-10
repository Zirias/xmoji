#include "xselection.h"

#include "object.h"
#include "timer.h"
#include "unistr.h"
#include "window.h"
#include "x11adapter.h"

#include <poser/core.h>
#include <stdlib.h>
#include <string.h>

#define MAXREQUESTORS 16
#define REQUESTTIMEOUT 5000

C_CLASS_DECL(XSelectionRequest);

struct XSelectionRequest
{
    XSelection *selection;
    XSelectionRequest *next;
    XSelectionRequest *parent;
    XSelectionRequest *subreqs;
    Timer *timeout;
    void *data;
    size_t datalen;
    size_t datapos;
    xcb_atom_t property;
    xcb_atom_t proptype;
    xcb_atom_t target;
    xcb_window_t requestor;
    xcb_timestamp_t time;
    uint16_t subidx;
    uint8_t propformat;
    uint8_t sendincr;
};

static void XSelectionRequest_reject(XSelection *selection,
	xcb_window_t requestor, xcb_atom_t target, xcb_timestamp_t time);
static void XSelectionRequest_done(XSelectionRequest *self, int destroy);
static void XSelectionRequest_abort(XSelectionRequest *self);
static void XSelectionRequest_timedout(
	void *receiver, void *sender, void *args);
static void XSelectionRequest_checkError(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error);
static void XSelectionRequest_propertyChanged(
	void *receiver, void *sender, void *args);
static void XSelectionRequest_subReject(XSelectionRequest *self, uint16_t idx);
static void XSelectionRequest_nextMulti(XSelectionRequest *self);
static void XSelectionRequest_startMulti(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error);
static void XSelectionRequest_start(XSelectionRequest *self);
static XSelectionRequest *XSelectionRequest_create(XSelection *selection,
	xcb_selection_request_event_t *ev, XSelectionRequest *parent);

struct XSelection
{
    Window *w;
    Widget *requestor;
    Widget *owner;
    Widget *newOwner;
    XSelectionRequest *requests[MAXREQUESTORS];
    size_t maxproplen;
    XSelectionCallback received;
    XSelectionContent content;
    XSelectionContent newContent;
    XSelectionType requestType;
    xcb_timestamp_t ownedTime;
    xcb_atom_t name;
    unsigned requestsnum;
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

static void clearSelection(XSelection *self)
{
    switch (self->content.type)
    {
	case XST_TEXT:
	    UniStr_destroy(self->content.data);
	    break;

	default:
	    break;
    }
    self->owner = 0;
    self->content = (XSelectionContent){0, XST_NONE};
    self->ownedTime = 0;
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

static void selectionClear(void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelection *self = receiver;
    xcb_selection_clear_event_t *ev = args;
    if (ev->selection != self->name) return;
    clearSelection(self);
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

static void XSelectionRequest_reject(XSelection *selection,
	xcb_window_t requestor, xcb_atom_t target, xcb_timestamp_t time)
{
    xcb_selection_notify_event_t ev = {
	.response_type = XCB_SELECTION_NOTIFY,
	.pad0 = 0,
	.sequence = 0,
	.time = time,
	.requestor = requestor,
	.selection = selection->name,
	.target = target,
	.property = 0
    };
    CHECK(xcb_send_event(X11Adapter_connection(), 0, requestor,
		0, (const char *)&ev),
	    "Cannot send rejection notification for 0x%x",
	    (unsigned)Window_id(selection->w));
}

static void XSelectionRequest_done(XSelectionRequest *self, int destroy)
{
    if (destroy && !self) return;
    Timer_destroy(self->timeout);
    free(self->data);
    if (self->sendincr)
    {
	PSC_Event_unregister(X11Adapter_propertyNotify(), self,
		XSelectionRequest_propertyChanged, self->requestor);
    }
    if (destroy)
    {
	XSelectionRequest_done(self->subreqs, 1);
	XSelectionRequest_done(self->next, 1);
	free(self);
	return;
    }
    if (self->parent)
    {
	XSelectionRequest *parent = self->parent;
	parent->subreqs = self->next;
	free(self);
	XSelectionRequest_nextMulti(parent);
	return;
    }
    XSelection *selection = self->selection;
    unsigned idx = 0;
    for (; idx < MAXREQUESTORS; ++idx)
    {
	if (selection->requests[idx] == self) break;
    }
    if (idx == MAXREQUESTORS)
    {
	PSC_Service_panic("BUG: Unknown selection request completed!");
    }
    selection->requests[idx] = self->next;
    if (!self->next)
    {
	uint32_t propnotifymask = 0;
	CHECK(xcb_change_window_attributes(X11Adapter_connection(),
		    self->requestor, XCB_CW_EVENT_MASK, &propnotifymask),
		"Cannot unlisten for requestor events on 0x%x",
		(unsigned)Window_id(self->selection->w));
    }
    PSC_Log_fmt(PSC_L_DEBUG, "Selection request destroyed for 0x%x",
	    self->requestor);
    free(self);
    if (selection->requests[idx])
    {
	XSelectionRequest_start(selection->requests[idx]);
    }
    else --selection->requestsnum;
}

static void XSelectionRequest_abort(XSelectionRequest *self)
{
    if (!self->datapos)
    {
	if (self->parent) XSelectionRequest_subReject(
		self->parent, self->subidx);
	else XSelectionRequest_reject(self->selection,
		self->requestor, self->target, self->time);
    }
    XSelectionRequest_done(self, 0);
}

static void XSelectionRequest_timedout(
	void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    XSelectionRequest *self = receiver;
    PSC_Log_fmt(PSC_L_INFO, "Selection request from 0x%x timed out",
	    self->requestor);
    XSelectionRequest_abort(self);
}

static void XSelectionRequest_checkError(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    XSelectionRequest *self = obj;
    if (error)
    {
	PSC_Log_fmt(PSC_L_WARNING,
		"Writing property to selection requestor 0x%x failed",
		(unsigned)self->requestor);
	XSelectionRequest_abort(self);
	return;
    }
    if (!self->datapos && !self->subreqs)
    {
	xcb_selection_notify_event_t ev = {
	    .response_type = XCB_SELECTION_NOTIFY,
	    .pad0 = 0,
	    .sequence = 0,
	    .time = self->time,
	    .requestor = self->requestor,
	    .selection = self->selection->name,
	    .target = self->target,
	    .property = self->property
	};
	CHECK(xcb_send_event(X11Adapter_connection(), 0, self->requestor,
		    0, (const char *)&ev),
		"Cannot send selection notification for 0x%x",
		(unsigned)Window_id(self->selection->w));
	if (!self->sendincr)
	{
	    XSelectionRequest_done(self, 0);
	    return;
	}
    }
    if (self->parent)
    {
	// incremental send in progress, detach from subrequests
	self->parent->subreqs = self->next;
	XSelectionRequest *parent = self->parent;
	XSelectionRequest *last = parent;
	while (last->next) last = last->next;
	last->next = self;
	XSelectionRequest_nextMulti(parent);
    }
}

static void XSelectionRequest_propertyChanged(
	void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelectionRequest *self = receiver;
    xcb_property_notify_event_t *ev = args;
    if (!self->sendincr
	    || ev->atom != self->property
	    || ev->state != XCB_PROPERTY_DELETE) return;
    uint32_t chunksz = self->selection->maxproplen;
    if (self->datalen - self->datapos < chunksz)
    {
	chunksz = self->datalen - self->datapos;
    }
    AWAIT(xcb_change_property(X11Adapter_connection(), XCB_PROP_MODE_APPEND,
		self->requestor, self->property, self->proptype,
		self->propformat, chunksz,
		(const char *)self->data + self->datapos),
	    self, XSelectionRequest_checkError);
    PSC_Log_fmt(PSC_L_DEBUG, "Incremental transfer to 0x%x, sending %u bytes",
	    (unsigned)self->requestor, (unsigned)chunksz);
    self->datapos += chunksz;
    if (!chunksz) XSelectionRequest_done(self, 0);
}

static void XSelectionRequest_subReject(XSelectionRequest *self, uint16_t idx)
{
    xcb_atom_t (*subspec)[2] = self->data;
    subspec[idx][1] = 0;
}

static void XSelectionRequest_nextMulti(XSelectionRequest *self)
{
    if (self->subreqs) XSelectionRequest_start(self->subreqs);
    else AWAIT(xcb_change_property(X11Adapter_connection(),
		XCB_PROP_MODE_REPLACE, self->requestor, self->property,
		self->proptype, self->propformat, self->datalen, self->data),
	    self, XSelectionRequest_checkError);
}

static void XSelectionRequest_startMulti(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;

    XSelectionRequest *self = obj;
    xcb_get_property_reply_t *prop = reply;
    if (prop) self->datalen = xcb_get_property_value_length(prop) << 2;
    if (error || !self->datalen)
    {
	PSC_Log_fmt(PSC_L_WARNING,
		"Reading MULTIPLE request from 0x%x failed",
		(unsigned)self->requestor);
	XSelectionRequest_abort(self);
    }

    self->data = PSC_malloc(self->datalen);
    memcpy(self->data, xcb_get_property_value(prop), self->datalen);
    XSelectionRequest *lastreq = 0;
    xcb_atom_t (*subspec)[2] = self->data;
    xcb_selection_request_event_t subev = {
	.response_type = 0,
	.pad0 = 0,
	.sequence = 0,
	.time = self->time,
	.owner = 0,
	.requestor = self->requestor,
	.selection = self->selection->name,
	.target = 0,
	.property = 0
    };
    size_t nreqs = self->datalen >> 3;
    for (size_t i = 0; i < nreqs; ++i)
    {
	subev.target = subspec[i][0];
	subev.property = subspec[i][1];
	XSelectionRequest *req = XSelectionRequest_create(
		self->selection, &subev, self);
	if (req)
	{
	    req->subidx = i;
	    if (!self->subreqs) self->subreqs = req;
	    else lastreq->next = req;
	    lastreq = req;
	}
	else subspec[i][1] = 0;
    }
    if (!self->subreqs)
    {
	XSelectionRequest_abort(self);
	return;
    }

    XSelectionRequest_start(self->subreqs);
}

static void XSelectionRequest_start(XSelectionRequest *self)
{
    xcb_connection_t *c = X11Adapter_connection();
    if (self->target == A(MULTIPLE))
    {
	AWAIT(xcb_get_property(c, 0, self->requestor, self->property,
		    A(ATOM_PAIR), 0, self->selection->maxproplen >> 2),
		self, XSelectionRequest_startMulti);
	return;
    }
    if (self->datalen > self->selection->maxproplen)
    {
	self->sendincr = 1;
	uint32_t incr = (uint32_t)-1;
	if (self->datalen < incr) incr = self->datalen;
	uint32_t propnotifymask = XCB_EVENT_MASK_PROPERTY_CHANGE;
	CHECK(xcb_change_window_attributes(c, self->requestor,
		    XCB_CW_EVENT_MASK, &propnotifymask),
		"Cannot listen for requestor events on 0x%x",
		(unsigned)Window_id(self->selection->w));
	PSC_Event_register(X11Adapter_propertyNotify(), self,
		XSelectionRequest_propertyChanged, self->requestor);
	self->timeout = Timer_create();
	PSC_Event_register(Timer_expired(self->timeout), self,
		XSelectionRequest_timedout, 0);
	Timer_start(self->timeout, REQUESTTIMEOUT);
	AWAIT(xcb_change_property(c, XCB_PROP_MODE_REPLACE, self->requestor,
		    self->property, A(INCR), 32, sizeof incr, &incr),
		self, XSelectionRequest_checkError);
	PSC_Log_fmt(PSC_L_DEBUG,
		"Starting incremental selection transfer to 0x%x",
		(unsigned)self->requestor);
    }
    else
    {
	self->sendincr = 0;
	AWAIT(xcb_change_property(c, XCB_PROP_MODE_REPLACE,
		    self->requestor, self->property, self->proptype,
		    self->propformat, self->datalen, self->data),
		self, XSelectionRequest_checkError);
	PSC_Log_fmt(PSC_L_DEBUG,
		"Transferring selection to 0x%x", (unsigned)self->requestor);
    }
}

static XSelectionRequest *XSelectionRequest_create(XSelection *selection,
	xcb_selection_request_event_t *ev, XSelectionRequest *parent)
{
    if ((ev->target == A(MULTIPLE) && !parent)
	    || ev->target == A(TARGETS)
	    || ev->target == A(TIMESTAMP)
	    || (ev->target == XCB_ATOM_STRING
		&& selection->content.type == XST_TEXT)
	    || (ev->target == A(TEXT)
		&& selection->content.type == XST_TEXT)
	    || (ev->target == A(UTF8_STRING)
		&& selection->content.type == XST_TEXT))
    {
	XSelectionRequest *self = PSC_malloc(sizeof *self);
	self->selection = selection;
	self->next = 0;
	self->parent = parent;
	self->subreqs = 0;
	self->timeout = 0;
	if (ev->target == A(MULTIPLE))
	{
	    self->data = 0;
	    self->datalen = 0;
	    self->proptype = A(ATOM_PAIR);
	    self->propformat = 32;
	}
	else if (ev->target == A(TARGETS))
	{
	    xcb_atom_t *targets = PSC_malloc(3 * sizeof *targets);
	    targets[0] = A(UTF8_STRING);
	    targets[1] = A(TEXT);
	    targets[2] = XCB_ATOM_STRING;
	    self->data = targets;
	    self->datalen = 3 * sizeof *targets;
	    self->proptype = XCB_ATOM_ATOM;
	    self->propformat = 32;
	}
	else if (ev->target == A(TIMESTAMP))
	{
	    self->data = PSC_malloc(sizeof selection->ownedTime);
	    memcpy(self->data, &selection->ownedTime,
		    sizeof selection->ownedTime);
	    self->datalen = sizeof selection->ownedTime;
	    self->proptype = XCB_ATOM_INTEGER;
	    self->propformat = 32;
	}
	else if (ev->target == XCB_ATOM_STRING)
	{
	    self->data = UniStr_toLatin1(selection->content.data);
	    self->datalen = UniStr_len(selection->content.data);
	    self->proptype = XCB_ATOM_STRING;
	    self->propformat = 8;
	}
	else if (ev->target == A(TEXT) || ev->target == A(UTF8_STRING))
	{
	    self->data = UniStr_toUtf8(selection->content.data,
		    &self->datalen);
	    self->proptype = A(UTF8_STRING);
	    self->propformat = 8;
	}
	self->datapos = 0;
	self->property = ev->property;
	self->target = ev->target;
	self->requestor = ev->requestor;
	self->time = ev->time;
	self->subidx = 0;
	PSC_Log_fmt(PSC_L_DEBUG, "Selection request created for 0x%x",
		self->requestor);
	return self;
    }
    if (!parent) XSelectionRequest_reject(selection,
	    ev->requestor, ev->target, ev->time);
    return 0;
}

static void selectionRequest(void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelection *self = receiver;
    xcb_selection_request_event_t *ev = args;
    if (ev->selection != self->name) return;

    if (self->content.type == XST_NONE)
    {
	XSelectionRequest_reject(self, ev->requestor, ev->target, ev->time);
	return;
    }

    XSelectionRequest *prev = 0;
    for (unsigned i = 0; i < MAXREQUESTORS; ++i)
    {
	if (self->requests[i] && self->requests[i]->requestor == ev->requestor)
	{
	    prev = self->requests[i];
	    while (prev->next) prev = prev->next;
	    break;
	}
    }
    if (!prev && self->requestsnum == MAXREQUESTORS)
    {
	XSelectionRequest_reject(self, ev->requestor, ev->target, ev->time);
	return;
    }

    XSelectionRequest *req = XSelectionRequest_create(self, ev, 0);
    if (!req) return;

    if (prev) prev->next = req;
    else for (unsigned i = 0; i < MAXREQUESTORS; ++i)
    {
	if (!self->requests[i])
	{
	    self->requests[i] = req;
	    ++self->requestsnum;
	    XSelectionRequest_start(req);
	    break;
	}
    }
}

static void onSelectionOwnerSet(void *obj, unsigned sequence,
	void *reply, xcb_generic_error_t *error)
{
    (void)sequence;
    (void)reply;

    if (error)
    {
	XSelection *self = obj;
	PSC_Log_fmt(PSC_L_ERROR, "Cannot obtain selection ownership for 0x%x",
		Window_id(self->w));
	clearSelection(self);
    }
}

static void ownPropertyNotify(void *receiver, void *sender, void *args)
{
    (void)sender;

    XSelection *self = receiver;
    xcb_property_notify_event_t *ev = args;
    if (ev->atom != A(WM_CLASS)) return;
    if (!self->newOwner || self->newContent.type == XST_NONE) return;
    switch (self->content.type)
    {
	case XST_TEXT:
	    UniStr_destroy(self->content.data);
	    break;

	default:
	    break;
    }
    self->owner = self->newOwner;
    self->content = self->newContent;
    self->ownedTime = ev->time;
    self->newOwner = 0;
    self->newContent = (XSelectionContent){0, XST_NONE};
    AWAIT(xcb_set_selection_owner(X11Adapter_connection(),
		Window_id(self->w), self->name, self->ownedTime),
	    self, onSelectionOwnerSet);
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
    self->maxproplen = X11Adapter_maxRequestSize()
	- sizeof(xcb_change_property_request_t);
    PSC_Event_register(X11Adapter_propertyNotify(), self,
	    ownPropertyNotify, Window_id(w));
    PSC_Event_register(X11Adapter_selectionClear(), self,
	    selectionClear, Window_id(w));
    PSC_Event_register(X11Adapter_selectionNotify(), self,
	    selectionNotify, Window_id(w));
    PSC_Event_register(X11Adapter_selectionRequest(), self,
	    selectionRequest, Window_id(w));
    return self;
}

void XSelection_request(XSelection *self, XSelectionType type,
	Widget *widget, XSelectionCallback received)
{
    if (self->content.type != XST_NONE)
    {
	if (self->content.type & type) received(widget, self->content);
	else received(widget, (XSelectionContent){0, XST_NONE});
	return;
    }
    if (self->requestor) nothingReceived(self);
    self->requestor = Object_ref(widget);
    self->received = received;
    self->requestType = type;
    AWAIT(xcb_convert_selection(X11Adapter_connection(), Window_id(self->w),
		self->name, A(TARGETS), self->name, XCB_CURRENT_TIME),
	    self, onSelectionConverted);
}

void XSelection_publish(XSelection *self, Widget *owner,
	XSelectionContent content)
{
    switch (self->newContent.type)
    {
	case XST_TEXT:
	    UniStr_destroy(self->newContent.data);
	    break;

	default:
	    break;
    }
    switch (content.type)
    {
	case XST_TEXT:
	    self->newContent.data = UniStr_ref(content.data);
	    break;

	default:
	    self->newContent.data = 0;
	    break;
    }
    self->newOwner = owner;
    if (!self->owner || owner != self->owner
	    || content.type != self->content.type)
    {
	CHECK(xcb_change_property(X11Adapter_connection(),
		    XCB_PROP_MODE_APPEND, Window_id(self->w), A(WM_CLASS),
		    XCB_ATOM_STRING, 8, 0, 0),
		"Cannot change property on 0x%x",
		(unsigned)Window_id(self->w));
	self->newContent.type = content.type;
    }
}

void XSelection_destroy(XSelection *self)
{
    if (!self) return;
    if (self->requestsnum) for (unsigned i = 0; i < MAXREQUESTORS; ++i)
    {
	XSelectionRequest_done(self->requests[i], 1);
    }
    if (self->requestor) nothingReceived(self);
    PSC_Event_unregister(X11Adapter_selectionRequest(), self,
	    selectionRequest, Window_id(self->w));
    PSC_Event_unregister(X11Adapter_selectionNotify(), self,
	    selectionNotify, Window_id(self->w));
    PSC_Event_unregister(X11Adapter_selectionClear(), self,
	    selectionClear, Window_id(self->w));
    PSC_Event_unregister(X11Adapter_propertyNotify(), self,
	    ownPropertyNotify, Window_id(self->w));
    free(self);
}


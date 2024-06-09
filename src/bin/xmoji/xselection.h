#ifndef XMOJI_XSELECTION_H
#define XMOJI_XSELECTION_H

#include <poser/decl.h>

C_CLASS_DECL(Widget);
C_CLASS_DECL(Window);
C_CLASS_DECL(XSelection);

typedef enum XSelectionName
{
    XSN_PRIMARY,
    XSN_CLIPBOARD
} XSelectionName;

typedef enum XSelectionType
{
    XST_NONE	    = 0,	// data is NULL
    XST_TEXT	    = 1 << 0,	// data is UniStr
} XSelectionType;

typedef struct XSelectionContent
{
    void *data;
    XSelectionType type;
} XSelectionContent;

typedef void (*XSelectionCallback)(Widget *widget, XSelectionContent content);

XSelection *XSelection_create(Window *w, XSelectionName name)
    ATTR_NONNULL((1));
void XSelection_request(XSelection *self, XSelectionType type,
	Widget *widget, XSelectionCallback received)
    CMETHOD ATTR_NONNULL((4));
void XSelection_publish(XSelection *self, Widget *owner,
	XSelectionContent content)
    CMETHOD;
void XSelection_destroy(XSelection *self);

#endif

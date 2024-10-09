#ifndef XMOJI_XDGOPEN_H
#define XMOJI_XDGOPEN_H

#include <poser/decl.h>

typedef enum XdgOpenError
{
    XOE_OK,		/* no error, errorHandler isn't called */
    XOE_SYSTEM,		/* unspecified system error, e.g. fork() */
    XOE_TOOLNOTFOUND,	/* couldn't find the xdg-open tool */
    XOE_EXEC		/* error executing xdg-open */
} XdgOpenError;

typedef void (*XdgOpenErrorHandler)(void *ctx,
	const char *url, XdgOpenError error);

void xdgOpen(const char *url, void *ctx, XdgOpenErrorHandler errorHandler)
    ATTR_NONNULL((1));

#endif

#ifndef XMOJI_XRDB_H
#define XMOJI_XRDB_H

#include <poser/decl.h>
#include <stddef.h>

C_CLASS_DECL(XRdb);

#define XRDB_KEYLEN 5
typedef const char *XRdbKey[XRDB_KEYLEN];
#define XRdbKey(...) (XRdbKey){ __VA_ARGS__ }

XRdb *XRdb_create(const char *str, size_t strlen,
	const char *className, const char *instanceName);
void XRdb_register(XRdb *self, const char *className, const char *instanceName)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3));
const char *XRdb_value(const XRdb *self, XRdbKey key)
    CMETHOD;
void XRdb_destroy(XRdb *self);

#endif

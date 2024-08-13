#ifndef XMOJI_HYPERLINK_H
#define XMOJI_HYPERLINK_H

#include "textlabel.h"

typedef struct MetaHyperLink
{
    MetaTextLabel base;
} MetaHyperLink;

#define MetaHyperLink_init(...) { \
    .base = MetaTextLabel_init(__VA_ARGS__) \
}

C_CLASS_DECL(HyperLink);

HyperLink *HyperLink_createBase(void *derived, const char *name, void *parent);
#define HyperLink_create(...) HyperLink_createBase(0, __VA_ARGS__)
const char *HyperLink_link(const void *self) CMETHOD;
void HyperLink_setLink(void *self, const char *link) CMETHOD;

#endif

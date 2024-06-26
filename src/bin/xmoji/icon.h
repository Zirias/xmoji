#ifndef XMOJI_ICON_H
#define XMOJI_ICON_H

#include <poser/decl.h>

C_CLASS_DECL(Icon);
C_CLASS_DECL(Pixmap);
C_CLASS_DECL(Window);

Icon *Icon_create(void);
Icon *Icon_ref(Icon *icon) ATTR_NONNULL((1));
void Icon_add(Icon *self, Pixmap *pixmap) CMETHOD ATTR_NONNULL((2));
void Icon_apply(Icon *self, Window *window) CMETHOD ATTR_NONNULL((2));
void Icon_destroy(Icon *self);

#endif

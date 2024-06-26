#ifndef XMOJI_IMAGELABEL_H
#define XMOJI_IMAGELABEL_H

#include "widget.h"

typedef struct MetaImageLabel
{
    MetaWidget base;
} MetaImageLabel;

#define MetaImageLabel_init(...) { \
    .base = MetaWidget_init(__VA_ARGS__) \
}

C_CLASS_DECL(ImageLabel);
C_CLASS_DECL(Pixmap);

ImageLabel *ImageLabel_createBase(void *derived,
	const char *name, void *parent);
#define ImageLabel_create(...) ImageLabel_createBase(0, __VA_ARGS__)
Pixmap *ImageLabel_pixmap(const void *self) CMETHOD;
void ImageLabel_setPixmap(void *self, Pixmap *pixmap) CMETHOD;

#endif

#include "pixmap.h"

#include "x11adapter.h"

#include <png.h>
#include <poser/core.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_image.h>

struct Pixmap
{
    unsigned char *data;
    size_t datasz;
    Size size;
    xcb_pixmap_t pixmap;
    xcb_render_picture_t picture;
    unsigned refcnt;
};

Pixmap *Pixmap_createFromPng(const unsigned char *data, size_t datasz)
{
    png_image png;
    png_bytep buffer = 0;
    memset(&png, 0, sizeof png);

    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png, data, datasz)) goto error;
    size_t bufsz = PNG_IMAGE_SIZE(png);
    buffer = PSC_malloc(bufsz);
    png.format = PNG_FORMAT_BGRA;
    if (!png_image_finish_read(&png, 0, buffer, 0, 0)) goto error;

    Pixmap *self = PSC_malloc(sizeof *self);
    self->data = buffer;
    self->datasz = bufsz;
    self->size = (Size){png.width, png.height};
    self->pixmap = 0;
    self->picture = 0;
    self->refcnt = 1;

    png_image_free(&png);
    return self;

error:
    free(buffer);
    png_image_free(&png);
    return 0;
}

Pixmap *Pixmap_ref(Pixmap *pixmap)
{
    ++pixmap->refcnt;
    return pixmap;
}

Size Pixmap_size(const Pixmap *self)
{
    return self->size;
}

void Pixmap_render(Pixmap *self, xcb_render_picture_t picture, Pos pos)
{
    xcb_connection_t *c = X11Adapter_connection();
    xcb_screen_t *s = X11Adapter_screen();
    if (!self->pixmap)
    {
	xcb_image_t *imgbgr = xcb_image_create_native(c, self->size.width,
		self->size.height, XCB_IMAGE_FORMAT_Z_PIXMAP, 24, 0, 0, 0);
	if (!imgbgr) return;
	xcb_image_t *imgalpha = xcb_image_create_native(c, self->size.width,
		self->size.height, XCB_IMAGE_FORMAT_Z_PIXMAP, 8, 0, 0, 0);
	if (!imgalpha)
	{
	    xcb_image_destroy(imgbgr);
	    return;
	}
	for (uint16_t y = 0; y < self->size.height; ++y)
	{
	    const unsigned char *inrow = self->data + y * self->size.width * 4;
	    uint8_t *outrow = imgbgr->data + y * imgbgr->stride;
	    uint8_t *outalpha = imgalpha->data + y * imgalpha->stride;
	    for (uint16_t x = 0; x < self->size.width; ++x)
	    {
		outrow[4*x] = inrow[4*x];
		outrow[4*x+1] = inrow[4*x+1];
		outrow[4*x+2] = inrow[4*x+2];
		outalpha[x] = inrow[4*x+3];
	    }
	}
	xcb_pixmap_t pmbgr = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 24, pmbgr, s->root,
		    self->size.width, self->size.height),
		"Cannot create tmp bgr pixmap for 0x%x", (unsigned)picture);
	xcb_pixmap_t pmalpha = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 8, pmalpha, s->root,
		    self->size.width, self->size.height),
		"Cannot create tmp alpha pixmap for 0x%x", (unsigned)picture);
	xcb_gcontext_t gcbgr = xcb_generate_id(c);
	CHECK(xcb_create_gc(c, gcbgr, pmbgr, 0, 0),
		"Cannot create graphics context for 0x%x", (unsigned)picture);
	xcb_gcontext_t gcalpha = xcb_generate_id(c);
	CHECK(xcb_create_gc(c, gcalpha, pmalpha, 0, 0),
		"Cannot create graphics context for 0x%x", (unsigned)picture);
	CHECK(xcb_image_put(c, pmbgr, gcbgr, imgbgr, 0, 0, 0),
		"Cannot upload rgb image data for 0x%x", (unsigned)picture);
	CHECK(xcb_image_put(c, pmalpha, gcalpha, imgalpha, 0, 0, 0),
		"Cannot upload alpha image data for 0x%x", (unsigned)picture);
	xcb_free_gc(c, gcalpha);
	xcb_free_gc(c, gcbgr);
	xcb_image_destroy(imgalpha);
	xcb_image_destroy(imgbgr);
	xcb_render_picture_t picbgr = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, picbgr, pmbgr,
		    X11Adapter_rgbformat(), 0, 0),
		"Cannot create tmp bgr picture for 0x%x", (unsigned)picture);
	xcb_render_picture_t picalpha = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, picalpha, pmalpha,
		    X11Adapter_alphaformat(), 0, 0),
		"Cannot create tmp alpha picture for 0x%x", (unsigned)picture);
	self->pixmap = xcb_generate_id(c);
	CHECK(xcb_create_pixmap(c, 32, self->pixmap, s->root,
		    self->size.width, self->size.height),
		"Cannot create pixmap for 0x%x", (unsigned)picture);
	self->picture = xcb_generate_id(c);
	CHECK(xcb_render_create_picture(c, self->picture, self->pixmap,
		    X11Adapter_argbformat(), 0, 0),
		"Cannot create picture for 0x%x", (unsigned)picture);
	CHECK(xcb_render_composite(c, XCB_RENDER_PICT_OP_SRC,
		    picbgr, picalpha, self->picture, 0, 0, 0, 0, 0, 0,
		    self->size.width, self->size.height),
		"Cannot compose picture for 0x%x", (unsigned)picture);
	xcb_render_free_picture(c, picalpha);
	xcb_render_free_picture(c, picbgr);
	xcb_free_pixmap(c, pmalpha);
	xcb_free_pixmap(c, pmbgr);
    }
    CHECK(xcb_render_composite(c, XCB_RENDER_PICT_OP_OVER, self->picture, 0,
		picture, 0, 0, 0, 0, pos.x, pos.y,
		self->size.width, self->size.height),
	    "Cannot render pixmap to 0x%x", (unsigned)picture);
}

void Pixmap_destroy(Pixmap *self)
{
    if (!self) return;
    if (--self->refcnt) return;
    if (self->pixmap)
    {
	xcb_connection_t *c = X11Adapter_connection();
	xcb_render_free_picture(c, self->picture);
	xcb_free_pixmap(c, self->pixmap);
    }
    free(self->data);
    free(self);
}


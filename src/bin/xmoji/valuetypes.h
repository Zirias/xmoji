#ifndef XMOJI_VALUETYPES_H
#define XMOJI_VALUETYPES_H

#include <xcb/render.h>

typedef uint32_t Color;

#define priv_Color_xcbr(x) (x)->red
#define priv_Color_xcbg(x) (x)->green
#define priv_Color_xcbb(x) (x)->blue
#define priv_Color_xcba(x) (x)->alpha
#define priv_Color_comp(c,a,s) _Generic(c, \
	Color *: ((*(c) >> s) & 0xffU), \
	xcb_render_color_t *: ((a((xcb_render_color_t *)c) + 0x80U) >> 8))
#define Color_red(c) priv_Color_comp(&(c), priv_Color_xcbr, 24)
#define Color_green(c) priv_Color_comp(&(c), priv_Color_xcbg, 16)
#define Color_blue(c) priv_Color_comp(&(c), priv_Color_xcbb, 8)
#define Color_alpha(c) priv_Color_comp(&(c), priv_Color_xcba, 0)

#define priv_Color_16(c) ((c) << 8 | (c))
#define Color_red16(c) priv_Color_16(Color_red(c))
#define Color_green16(c) priv_Color_16(Color_green(c))
#define Color_blue16(c) priv_Color_16(Color_blue(c))
#define Color_alpha16(c) priv_Color_16(Color_alpha(c))

#define Color_fromRgb(r,g,b) (((r)<<24)|((g)<<16)|((b)<<8)|0xffU)
#define Color_fromRgba(r,g,b,a) (((r)<<24)|((g)<<16)|((b)<<8)|(a))
#define Color_xcb(c) (xcb_render_color_t){ \
    .red = Color_red16(c), \
    .green = Color_green16(c), \
    .blue = Color_blue16(c), \
    .alpha = Color_alpha16(c) \
}
#define Color_setXcb(c,x) do { \
    (x)->red = Color_red16(c); \
    (x)->green = Color_green16(c); \
    (x)->blue = Color_blue16(c); \
    (x)->alpha = Color_alpha16(c); \
} while (0)

typedef struct Pos
{
    int16_t x;
    int16_t y;
} Pos;

typedef struct Size
{
    uint16_t width;
    uint16_t height;
} Size;

typedef struct Rect
{
    Pos pos;
    Size size;
} Rect;

#define Rect_overlaps(r1,r2) (( \
	(r2.pos.x >= r1.pos.x && r2.pos.x < r1.pos.x + r1.size.width) || \
	(r2.pos.x < r1.pos.x && r2.pos.x + r2.size.width >= r1.pos.x)) && ( \
	(r2.pos.y >= r1.pos.y && r2.pos.y < r1.pos.y + r1.size.height) || \
	(r2.pos.y < r1.pos.y && r2.pos.y + r2.size.height >= r1.pos.y)))

#define Rect_contains(r1,r2) (r2.pos.x >= r1.pos.x && r2.pos.y >= r1.pos.y \
	&& r2.size.width + (r2.pos.x - r1.pos.x) <= r1.size.width \
	&& r2.size.height + (r2.pos.y - r1.pos.y) <= r1.size.height)

#define Rect_containsPos(r,p) (p.x >= r.pos.x && p.x < r.pos.x + r.size.width \
	&& p.y >= r.pos.y && p.y < r.pos.y + r.size.height)

typedef struct Box
{
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} Box;

#define Rect_pad(r,b) ((Rect){ \
	.pos = { \
	    .x = r.pos.x + b.left, \
	    .y = r.pos.y + b.top \
	}, \
	.size = { \
	    .width = r.size.width - b.left - b.right, \
	    .height = r.size.height - b.top - b.bottom \
	} \
    })

#define Box_fromRect(r) ((Box){ \
	.left = r.pos.x, \
	.top = r.pos.y, \
	.right = r.pos.x + r.size.width, \
	.bottom = r.pos.y + r.size.height \
    })

typedef struct Selection
{
    unsigned start;
    unsigned len;
} Selection;

typedef enum Align
{
    AH_RIGHT	= 1 << 0,
    AH_CENTER	= 1 << 1,
    AV_BOTTOM	= 1 << 2,
    AV_MIDDLE	= 1 << 3
} Align;

typedef enum Expand
{
    EXPAND_NONE	= 0,
    EXPAND_X	= 1 << 0,
    EXPAND_Y	= 1 << 1
} Expand;

#endif

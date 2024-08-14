#ifndef XMOJI_CHAR32_H
#define XMOJI_CHAR32_H

#ifdef HAVE_CHAR32_T
#  include <uchar.h>
#else
#  include <stdint.h>
typedef uint_least32_t char32_t;
#endif

#endif

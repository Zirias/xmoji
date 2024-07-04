#ifndef XMOJI_MACROS_H
#define XMOJI_MACROS_H

#define _STR(x) #x
#define STR(x) _STR(x)

#define X_ENUM(a) a,
#define X_SZNM(a) { sizeof STR(a) - 1, STR(a) },

#define U8LPT(x) char(*)[sizeof x]
#define U32LPT(x) char32_t(*)[sizeof x >> 2 ? sizeof x >> 2 : 1]

#endif

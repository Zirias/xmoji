#ifndef XMOJI_MACROS_H
#define XMOJI_MACROS_H

#define _STR(x) #x
#define STR(x) _STR(x)

#define X_ENUM(a) a,
#define X_SZNM(a) { sizeof STR(a) - 1, STR(a) },

#define U8LPT(x) char(*)[sizeof x]
#define U32LPT(x) char32_t(*)[sizeof x >> 2 ? sizeof x >> 2 : 1]
#define U32LIT(x) _Generic(&x, \
	U8LPT(x): U ## x, \
	const U8LPT(x): U ## x, \
	default: x)

#endif

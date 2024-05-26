#ifndef XMOJI_SUPPRESS_H
#define XMOJI_SUPPRESS_H

#if defined(__clang__)
#  define xmoji___compiler clang
#elif defined(__GNUC__)
#  define xmoji___compiler GCC
#endif
#ifdef xmoji___compiler
#  define xmoji___pragma(x) _Pragma(#x)
#  define xmoji___diagprag1(x,y) xmoji___pragma(x diagnostic y)
#  define xmoji___diagprag(x) xmoji___diagprag1(xmoji___compiler, x)
#  define xmoji___suppress1(x) xmoji___diagprag(ignored x)
#  define xmoji___suppress(x) xmoji___suppress1(#x)
#  define SUPPRESS(x) xmoji___diagprag(push) xmoji___suppress(-W##x)
#  define ENDSUPPRESS xmoji___diagprag(pop)
#else
#  define SUPPRESS(x)
#  define ENDSUPPRESS
#endif

#endif

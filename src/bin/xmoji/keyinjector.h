#ifndef XMOJI_KEYINJECTOR_H
#define XMOJI_KEYINJECTOR_H

#include <poser/decl.h>

C_CLASS_DECL(UniStr);

typedef enum InjectorFlags
{
    IF_NONE	    = 0,
    IF_ADDSPACE	    = 1 << 0, /* Inject a space after each emoji */
    IF_ADDZWSPACE   = 1 << 1, /* Inject a zero-width space after each emoji */
    IF_EXTRAZWJ	    = 1 << 2  /* For a ZWJ sequence, add an extra ZWJ at the
				 beginning */
} InjectorFlags;

void KeyInjector_init(unsigned beforems, unsigned afterms,
	InjectorFlags flags);
void KeyInjector_inject(const UniStr *str);
void KeyInjector_done(void);

#endif

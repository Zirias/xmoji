#ifndef XMOJI_TIMER_H
#define XMOJI_TIMER_H

#include <poser/decl.h>

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(Timer);

Timer *Timer_create(void);
PSC_Event *Timer_expired(Timer *self) CMETHOD;
void Timer_start(Timer *self, unsigned interval_ms) CMETHOD;
#define Timer_stop(t) Timer_start(t, 0)
void Timer_destroy(Timer *self);

#endif

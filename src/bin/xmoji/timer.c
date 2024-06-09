#define _POSIX_C_SOURCE 200112L

#include "timer.h"

#include <poser/core.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct Timer
{
    PSC_Event *expired;
    timer_t timerid;
    int pipe[2];
};

static void expired(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Timer *self = receiver;
    char dummy;
    while (read(self->pipe[0], &dummy, 1) == 1)
    {
	PSC_Event_raise(self->expired, 0, 0);
    }
}

static void expirefunc(union sigval sv)
{
    Timer *self = sv.sival_ptr;
    static const char dummy = 0;
    write(self->pipe[1], &dummy, 1);
}

Timer *Timer_create(void)
{
    Timer *self = PSC_malloc(sizeof *self);
    memset(self, 0, sizeof *self);
    if (pipe(self->pipe) < 0) goto error;
    PSC_Event_register(PSC_Service_readyRead(), self, expired, self->pipe[0]);
    PSC_Service_registerRead(self->pipe[0]);
    struct sigevent ev = {
	.sigev_notify = SIGEV_THREAD,
	.sigev_signo = 0,
	.sigev_value = { .sival_ptr = self },
	.sigev_notify_function = expirefunc,
	.sigev_notify_attributes = 0
    };
    if (timer_create(CLOCK_MONOTONIC, &ev, &self->timerid) < 0) goto error;
    self->expired = PSC_Event_create(self);
    return self;

error:
    Timer_destroy(self);
    return 0;
}

void Timer_start(Timer *self, unsigned interval_ms)
{
    struct itimerspec itv = {
	.it_interval = {
	    .tv_sec = interval_ms / 1000U,
	    .tv_nsec = 1000000U * (interval_ms % 1000U) },
	.it_value = {
	    .tv_sec = interval_ms / 1000U,
	    .tv_nsec = 1000000U * (interval_ms % 1000U) }
    };
    timer_settime(self->timerid, 0, &itv, 0);
}

void Timer_destroy(Timer *self)
{
    if (!self) return;
    PSC_Event_destroy(self->expired);
    if (self->timerid) timer_delete(self->timerid);
    if (self->pipe[1])
    {
	PSC_Service_unregisterRead(self->pipe[0]);
	PSC_Event_unregister(PSC_Service_readyRead(), self, expired,
		self->pipe[0]);
	close(self->pipe[1]);
	close(self->pipe[0]);
    }
    free(self);
}


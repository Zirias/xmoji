#include "tooltip.h"

#include "textlabel.h"
#include "timer.h"
#include "window.h"

#include <poser/core.h>

static Timer *timer;
unsigned timerref;

struct Tooltip
{
    TextLabel *label;
    Widget *parent;
    Window *window;
    unsigned delay;
};

static void timeout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Tooltip *self = receiver;
    Timer_stop(timer);
    PSC_Event_unregister(Timer_expired(timer), self, timeout, 0);
    if (self->window)
    {
	Window_showTooltip(self->window, self->label, self->parent);
    }
}

Tooltip *Tooltip_create(const UniStr *text, Widget *parent, unsigned delay)
{
    if (!timerref++)
    {
	timer = Timer_create();
    }
    Tooltip *self = PSC_malloc(sizeof *self);
    self->label = TextLabel_create(0, 0);
    self->parent = parent;
    self->window = 0;
    self->delay = delay ? delay : 2000;

    TextLabel_setText(self->label, text);
    TextLabel_setColor(self->label, COLOR_TOOLTIP);
    Widget_setBackground(self->label, 1, COLOR_BG_TOOLTIP);
    Widget_show(self->label);

    return self;
}

void Tooltip_activate(Tooltip *self, Window *window)
{
    self->window = window;
    PSC_Event_register(Timer_expired(timer), self, timeout, 0);
    Timer_start(timer, self->delay);
}

void Tooltip_cancel(Tooltip *self)
{
    self->window = 0;
    Timer_stop(timer);
    PSC_Event_unregister(Timer_expired(timer), self, timeout, 0);
}

void Tooltip_destroy(Tooltip *self)
{
    if (!self) return;
    Object_destroy(self->label);
    free(self);
    if (!--timerref)
    {
	Timer_destroy(timer);
    }
}


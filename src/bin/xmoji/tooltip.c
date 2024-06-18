#include "tooltip.h"

#include "textlabel.h"
#include "timer.h"
#include "window.h"

#include <poser/core.h>

struct Tooltip
{
    TextLabel *label;
    Timer *timer;
    Window *window;
};

static void timeout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Tooltip *self = receiver;
    Timer_stop(self->timer);
    if (self->window) Window_showTooltip(self->window, self->label);
}

Tooltip *Tooltip_create(const UniStr *text)
{
    Tooltip *self = PSC_malloc(sizeof *self);
    self->label = TextLabel_create(0, 0);
    self->timer = Timer_create();
    self->window = 0;

    TextLabel_setText(self->label, text);
    TextLabel_setColor(self->label, COLOR_SELECTED);
    Widget_setBackground(self->label, 1, COLOR_BG_SELECTED);
    Widget_show(self->label);

    PSC_Event_register(Timer_expired(self->timer), self, timeout, 0);

    return self;
}

void Tooltip_activate(Tooltip *self, Window *window)
{
    self->window = window;
    Timer_start(self->timer, 3000);
}

void Tooltip_cancel(Tooltip *self)
{
    self->window = 0;
    Timer_stop(self->timer);
}

void Tooltip_destroy(Tooltip *self)
{
    if (!self) return;
    PSC_Event_unregister(Timer_expired(self->timer), self, timeout, 0);
    Timer_destroy(self->timer);
    Object_destroy(self->label);
    free(self);
}


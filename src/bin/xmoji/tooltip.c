#include "tooltip.h"

#include "textlabel.h"
#include "window.h"

#include <poser/core.h>

struct Tooltip
{
    TextLabel *label;
    Widget *parent;
    Window *window;
    PSC_Timer *timer;
};

static void timeout(void *receiver, void *sender, void *args)
{
    (void)sender;
    (void)args;

    Tooltip *self = receiver;
    PSC_Timer_stop(self->timer);
    if (self->window)
    {
	Window_showTooltip(self->window, self->label, self->parent);
    }
}

Tooltip *Tooltip_create(const UniStr *text, Widget *parent, unsigned delay)
{
    Tooltip *self = PSC_malloc(sizeof *self);
    self->label = TextLabel_create(0, 0);
    self->parent = parent;
    self->window = 0;
    self->timer = PSC_Timer_create();
    PSC_Timer_setMs(self->timer, delay ? delay : 2000);
    PSC_Event_register(PSC_Timer_expired(self->timer), self, timeout, 0);

    TextLabel_setText(self->label, text);
    TextLabel_setColor(self->label, COLOR_TOOLTIP);
    Widget_setBackground(self->label, 1, COLOR_BG_TOOLTIP);
    Widget_show(self->label);

    return self;
}

void Tooltip_activate(Tooltip *self, Window *window)
{
    self->window = window;
    PSC_Timer_start(self->timer);
}

void Tooltip_cancel(Tooltip *self)
{
    self->window = 0;
    PSC_Timer_stop(self->timer);
}

void Tooltip_destroy(Tooltip *self)
{
    if (!self) return;
    PSC_Timer_destroy(self->timer);
    Object_destroy(self->label);
    free(self);
}


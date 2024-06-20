#include "command.h"

#include "unistr.h"
#include "widget.h"

#include <poser/core.h>
#include <stdlib.h>

static void destroy(void *obj);

static MetaCommand mo = MetaCommand_init("Command", destroy);

typedef struct EventEntry
{
    Command *command;
    void *widget;
    PSC_Event *event;
    WidgetEvent accessor;
} EventEntry;

struct Command
{
    Object base;
    UniStr *name;
    UniStr *description;
    PSC_Event *triggered;
    PSC_List *attachedEvents;
};

static void trigger(void *receiver, void *sender, void *args)
{
    Command *self = receiver;
    CommandTriggeredEventArgs ea = {
	.sender = sender,
	.args = args
    };
    PSC_Event_raise(self->triggered, 0, &ea);
}

static void destroyEntry(void *obj)
{
    EventEntry *entry = obj;
    PSC_Event_unregister(entry->event, entry->command, trigger, 0);
    Object_destroy(entry->widget);
    free(entry);
}

static void destroy(void *obj)
{
    Command *self = Object_instance(obj);
    PSC_List_destroy(self->attachedEvents);
    PSC_Event_destroy(self->triggered);
    UniStr_destroy(self->description);
    UniStr_destroy(self->name);
    free(self);
}

Command *Command_createBase(void *derived,
	const UniStr *name, const UniStr *description, void *parent)
{
    Command *self = PSC_malloc(sizeof *self);
    CREATEBASE(Object);
    self->name = name ? UniStr_ref(name) : 0;
    self->description = description ? UniStr_ref(description) : 0;
    self->triggered = PSC_Event_create(self);
    self->attachedEvents = PSC_List_create();
    if (parent) Object_own(parent, self);
    return self;
}

PSC_Event *Command_triggered(void *self)
{
    Command *cmd = Object_instance(self);
    return cmd->triggered;
}

const UniStr *Command_name(const void *self)
{
    const Command *cmd = Object_instance(self);
    return cmd->name;
}

const UniStr *Command_description(const void *self)
{
    const Command *cmd = Object_instance(self);
    return cmd->description;
}

void Command_trigger(void *self)
{
    Command *cmd = Object_instance(self);
    trigger(cmd, cmd, 0);
}

void Command_attach(void *self, void *widget, WidgetEvent event)
{
    Widget *w = Widget_cast(widget);
    if (!w) return;
    Command *cmd = Object_instance(self);
    for (size_t i = 0; i < PSC_List_size(cmd->attachedEvents); ++i)
    {
	EventEntry *entry = PSC_List_at(cmd->attachedEvents, i);
	if (entry->widget == w && entry->accessor == event) return;
    }
    EventEntry *entry = PSC_malloc(sizeof *entry);
    entry->command = cmd;
    entry->widget = Object_ref(w);
    entry->event = event(w);
    entry->accessor = event;
    PSC_Event_register(entry->event, cmd, trigger, 0);
    PSC_List_append(cmd->attachedEvents, entry, destroyEntry);
}

struct EntryMatch {
    void *widget;
    WidgetEvent event;
};

static int entryMatches(void *obj, const void *arg)
{
    EventEntry *entry = obj;
    const struct EntryMatch *match = arg;
    return (entry->widget == match->widget && entry->accessor == match->event);
}

void Command_detach(void *self, void *widget, WidgetEvent event)
{
    Widget *w = Widget_cast(widget);
    if (!w) return;
    Command *cmd = Object_instance(self);
    struct EntryMatch match = {
	.widget = w,
	.event = event
    };
    PSC_List_removeAll(cmd->attachedEvents, entryMatches, &match);
}


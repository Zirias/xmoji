#ifndef XMOJI_COMMAND_H
#define XMOJI_COMMAND_H

#include "object.h"

#include <poser/decl.h>

C_CLASS_DECL(Command);
C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(UniStr);

typedef struct CommandTriggeredEventArgs
{
    void *sender;
    void *args;
} CommandTriggeredEventArgs;

typedef PSC_Event *(*WidgetEvent)(void *widget);

typedef struct MetaCommand
{
    MetaObject base;
} MetaCommand;

#define MetaCommand_init(...) { \
    .base = MetaObject_init(__VA_ARGS__) \
}

Command *Command_createBase(void *derived,
	const UniStr *name, const UniStr *description, void *parent);
#define Command_create(...) Command_createBase(0, __VA_ARGS__)
PSC_Event *Command_triggered(void *self) CMETHOD;
const UniStr *Command_name(const void *self) CMETHOD;
const UniStr *Command_description(const void *self) CMETHOD;
void Command_trigger(void *self) CMETHOD;
void Command_attach(void *self, void *widget, WidgetEvent event)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3));
void Command_detach(void *self, void *widget, WidgetEvent event)
    CMETHOD ATTR_NONNULL((2)) ATTR_NONNULL((3));

#endif
